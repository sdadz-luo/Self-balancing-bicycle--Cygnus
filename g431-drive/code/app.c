#include "headline.h"

/**
  * @file    app.c
  * @brief   应用初始化序列与主循环：调试串口就绪、复位原因打印、
  *          IWDG 放狗（app_init 序列末）与 30ms 窗口中段喂狗
  * @author  sdadz-luo
  */

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

/* 上次喂狗 tick：文件级是因为初值必须在 app_init 放狗后锚定，
   不能靠静态零初值（机理见 app_init 内锚定处注释） */
static uint32_t dog_ms;

void app_init(void)
{
    uart_init();
    protect_init(); /* freeze 位先于放狗配置：断点下 IWDG 不咬，现场完整 */
    reset_cause_print();
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
