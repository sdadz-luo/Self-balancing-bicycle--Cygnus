#include "headline.h"

/**
  * @file    app.c
  * @brief   应用初始化序列与主循环：上电自检（时钟、ADC 校准、SPI 总线、
  *          功率级 EN_GATE 与 DRV8301 配置读回、ADC 采样自检）、IWDG 放狗
  *          （序列末）与 30ms 窗口中段喂狗
  * @author  sdadz-luo
  */

/* 系统时钟目标值：HSE 8M → PLL → 170 MHz（.ioc 固化） */
#define APP_SYS_CLK_HZ 170000000U

/* EN_GATE 拉高到两片 nFAULT 释放的超时：手册 t_SPI_READY 5~10 ms，
   再留启动与上拉建立余量 */
#define APP_DRV_READY_TIMEOUT_MS 50U

/* 系统 LED：PC9，低电平点亮（初始 High = 灭，程序没跑起来时不会被误认为正常） */
#define APP_LED_SYS_PORT GPIOC
#define APP_LED_SYS_PIN  GPIO_PIN_9

/* 上电自检失败：串口打印步骤号 + LED 慢闪 N 次定位步号，阻塞停机。
   此刻 IWDG 尚未启动（序列末才放狗），死循环安全。若失败发生在
   EN_GATE 拉高之后，功率级处于使能态但 MOE=0、PWM 未启动，MOSFET 全关 */
static void app_fatal(uint8_t step)
{
    printf("fatal: step %u\r\n", (unsigned int)step);

    for (;;)
    {
        for (uint8_t i = 0U; i < step; i++)
        {
            HAL_GPIO_WritePin(APP_LED_SYS_PORT, APP_LED_SYS_PIN, GPIO_PIN_RESET);
            HAL_Delay(200U);
            HAL_GPIO_WritePin(APP_LED_SYS_PORT, APP_LED_SYS_PIN, GPIO_PIN_SET);
            HAL_Delay(200U);
        }
        HAL_Delay(1000U);
    }
}

/* 复位原因解码：上电第一时间读 RCC->CSR（排查偶发重启的第一手
   线索）。多标志可能并存——IWDG 复位会伴随内部 NRST 脉冲把
   PINRSTF 也置位，逐项列出而非单选 */
static void reset_cause_print(void)
{
    uint32_t csr = RCC->CSR;
    bool     first = true;

    printf("reset:");

    if ((csr & RCC_CSR_IWDGRSTF) != 0U)
    {
        printf(" IWDG");
        first = false;
    }
    if ((csr & RCC_CSR_WWDGRSTF) != 0U)
    {
        printf(" WWDG");
        first = false;
    }
    if ((csr & RCC_CSR_SFTRSTF) != 0U)
    {
        printf(" SFT");
        first = false;
    }
    if ((csr & RCC_CSR_BORRSTF) != 0U)
    {
        printf(" BOR");
        first = false;
    }
    if ((csr & RCC_CSR_PINRSTF) != 0U)
    {
        printf(" PIN");
        first = false;
    }
    if ((csr & RCC_CSR_OBLRSTF) != 0U)
    {
        printf(" OBL");
        first = false;
    }
    if ((csr & RCC_CSR_LPWRRSTF) != 0U)
    {
        printf(" LPWR");
        first = false;
    }
    if (first)
    {
        printf(" POR");
    }
    printf("\r\n");

    /* 读毕清标志，此后只反映本次复位周期内的新事件 */
    __HAL_RCC_CLEAR_RESET_FLAGS();
}

/* ADC 采样自检（交接文档 2026-11-23）：用软件替代万用表——
   VBUS 对照限流源、四路 SO 与 DC_CAL 零点、两片 DRV 状态。
   数值异常只打印不阻塞（阶段 0/1 不接母线时 VBUS=0 属正常），
   只有 HAL 超时（采样链路硬故障）才停机。
   须在 EN_GATE 拉高、PWM 未开时调用（设计方案 3.2 第 7 条） */
static void app_adc_selftest(void)
{
    float    vbus_v  = 0.0f;
    uint32_t vdda_mv = 0U;
    uint16_t so[ADC_SO_COUNT] = {0};
    uint16_t dc[ADC_SO_COUNT] = {0};

    if (!adc_vbus_read(&vbus_v, &vdda_mv))
    {
        app_fatal(5U);
    }
    if (!adc_phase_offset_read(so))
    {
        app_fatal(5U);
    }
    if (!adc_dc_cal(dc))
    {
        app_fatal(5U);
    }

    /* 整数 mV / 原始码打印：不引入 MicroLIB 浮点 printf */
    printf("adc: vdda=%lumV vbus=%lumV\r\n",
           (unsigned long)vdda_mv,
           (unsigned long)((uint32_t)(vbus_v * 1000.0f + 0.5f)));
    printf("adc: so=%u %u %u %u\r\n",
           (unsigned int)so[ADC_SO_M1_IA], (unsigned int)so[ADC_SO_M1_IB],
           (unsigned int)so[ADC_SO_M2_IA], (unsigned int)so[ADC_SO_M2_IB]);
    printf("adc: dccal=%u %u %u %u\r\n",
           (unsigned int)dc[ADC_SO_M1_IA], (unsigned int)dc[ADC_SO_M1_IB],
           (unsigned int)dc[ADC_SO_M2_IA], (unsigned int)dc[ADC_SO_M2_IB]);

    /* OC 位读后自清，必须走 drv8301_status 一次取全（设计方案 3.2 第 8 条） */
    for (uint8_t dev = (uint8_t)DRV_DEV_1; dev < (uint8_t)DRV_DEV_COUNT; dev++)
    {
        DrvStatus st = {0};

        if (!drv8301_status((DrvDev)dev, &st))
        {
            app_fatal(5U);
        }
        printf("drv%u: st1=%04X st2=%04X\r\n",
               (unsigned int)(dev + 1U),
               (unsigned int)st.status1,
               (unsigned int)st.status2);
    }
}

/* 上次喂狗 tick：文件级是因为初值必须在 app_init 放狗后锚定，
   不能靠静态零初值（机理见 app_init 内锚定处注释） */
static uint32_t dog_ms;

void app_init(void)
{
    uart_init();
    protect_init(); /* freeze 位先于放狗配置：断点下 IWDG 不咬，现场完整 */
    reset_cause_print();

    /* 时钟自检：时钟不对跑 FOC 全是错的，此步失败不继续 */
    if (HAL_RCC_GetSysClockFreq() != APP_SYS_CLK_HZ)
    {
        app_fatal(1U);
    }

    /* ADC 校准：上电一次，规则组与注入组共用（须在 ADC 使能之前） */
    if (!adc_init())
    {
        app_fatal(2U);
    }

    /* SPI 总线：四路 CS 拉高、两条总线置编码器常态档 */
    spi_bus_init();

    /* 功率级上电序列（设计方案 3.2 第 7 条）：
       EN_GATE → 等 nFAULT 释放 → 两片 DRV8301 配置并读回校验 → ADC 采样自检。
       先配再开 PWM，任何情况下不例外；EN_GATE 低电平期间 SPI 不可用 */
    drv8301_enable();
    if (!drv8301_wait_ready(APP_DRV_READY_TIMEOUT_MS))
    {
        app_fatal(3U);
    }
    for (uint8_t dev = (uint8_t)DRV_DEV_1; dev < (uint8_t)DRV_DEV_COUNT; dev++)
    {
        if (!drv8301_init((DrvDev)dev))
        {
            app_fatal(4U);
        }
    }

    /* ADC 采样自检：须在 EN_GATE 拉高、PWM 未开时（设计方案 3.2 第 7 条） */
    app_adc_selftest();

    printf("init: ok\r\n");

    /* 放狗即启动（HAL_IWDG_Init 内部写启动键，无独立 Start API）——
       必须是序列末：此处调用时刻 = 看门狗开始倒数时刻（约定见端级
       CLAUDE.md，MX_IWDG_Init 不进 main.c 正是为守住这一行） */
    MX_IWDG_Init();
    /* 首喂基准锚定到放狗时刻：窗口判据是「狗上岗后 20 ms 内喂狗即
       复位」（WINR=20）。零初值会让首喂死在 tick=30 的绝对时刻——
       上电序列一旦超 10 ms（功率阶段等 nFAULT 就绪要 20~50 ms），
       首喂间隔不足 20 ms，上电即窗口违例、复位循环 */
    dog_ms = HAL_GetTick();
}

void app_main(void)
{
    uint32_t now = HAL_GetTick();

    /* 30ms 窗口中段喂狗：窗口 20~40ms，两侧各 10ms 余量，LSI 的
       RC 精度（±几%）也在余量内 */
    if ((now - dog_ms) >= 30U)
    {
        dog_ms = now;
        (void)HAL_IWDG_Refresh(&hiwdg);
    }
}
