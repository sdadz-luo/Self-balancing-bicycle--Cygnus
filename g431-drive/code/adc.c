#include "headline.h"

/* 句柄定义在 CubeMX 生成的 Core/Src/adc.c；与 Core/Inc/adc.h 同名、
   按名 include 无法命中那一份，在唯一使用处直接声明 */
extern ADC_HandleTypeDef hadc1; /* 电机 1：M1_IA/M1_IB（PA0/PA1，注入组） */
extern ADC_HandleTypeDef hadc2; /* 电机 2：M2_IA/M2_IB（PC0/PC1，注入组） */

/**
  * @file    adc.c
  * @brief   ADC 封装层：上电校准、规则组母线电压（VREFINT 反推 VDDA 校正）、
  *          注入组相电流零点与 DC_CAL 自检
  * @author  sdadz-luo
  */

/* 轮询超时：规则组 4 通道约 13.5 µs、注入组 2 通道约 0.9 µs（42.5 MHz），
   10 ms 只用于兜底硬件异常 */
#define ADC_POLL_TIMEOUT_MS 10U

/* VREFINT 出厂校准值（3.000 V 条件下），用于反推实际 VDDA。
   地址宏在 LL 头，按「各 .c 只 include headline.h」的约定这里直接定义 */
#define ADC_VREFINT_CAL_ADDR ((const uint16_t *)0x1FFF75AAUL)
#define ADC_VREFINT_CAL_MV   3000U

/* 母线分压 34.6k + 4.99k（实物原理图）：V_bus = V_adc × (34.6+4.99)/4.99，
   满量程 ≈26.2 V（参数与配置 3.2） */
#define ADC_VBUS_DIV_HIGH_KOHM 34.6f
#define ADC_VBUS_DIV_LOW_KOHM  4.99f

/* 规则组一次转换，按 rank 顺序返回 VBUS/NTC1/NTC2/VREFINT 原始码 */
static bool adc_reg_read(uint16_t raw[4])
{
    if (HAL_ADC_Start(&hadc1) != HAL_OK)
    {
        return false;
    }

    for (uint32_t i = 0U; i < 4U; i++)
    {
        if (HAL_ADC_PollForConversion(&hadc1, ADC_POLL_TIMEOUT_MS) != HAL_OK)
        {
            (void)HAL_ADC_Stop(&hadc1);
            return false;
        }
        raw[i] = (uint16_t)HAL_ADC_GetValue(&hadc1);
    }

    (void)HAL_ADC_Stop(&hadc1);
    return true;
}

/* 软件触发一次注入组并读两路。
   CubeMX 配的是外部触发（ADC1=T8_TRGO2 / ADC2=T1_TRGO2）：JEXTEN≠00 时
   写 JADSTART 只是武装、要等触发事件才转换（RM0440，HAL 注释同）——
   自检时 TIM 未运行，必须临时把 JEXTEN 切到 00（软件触发）才能立即启动，
   读完恢复原值。JQDIS=1（QueueInjectedContext=DISABLE）是 HAL 软件启动
   检查的前提，CubeMX 已配 */
static bool adc_inj_read(ADC_HandleTypeDef *hadc, uint16_t *a, uint16_t *b)
{
    uint32_t jexten = hadc->Instance->JSQR & ADC_JSQR_JEXTEN_Msk;

    MODIFY_REG(hadc->Instance->JSQR, ADC_JSQR_JEXTEN_Msk, 0U);

    bool ok = false;
    if (HAL_ADCEx_InjectedStart(hadc) == HAL_OK)
    {
        if (HAL_ADCEx_InjectedPollForConversion(hadc, ADC_POLL_TIMEOUT_MS) == HAL_OK)
        {
            *a = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_1);
            *b = (uint16_t)HAL_ADCEx_InjectedGetValue(hadc, ADC_INJECTED_RANK_2);
            ok = true;
        }
        (void)HAL_ADCEx_InjectedStop(hadc);
    }

    MODIFY_REG(hadc->Instance->JSQR, ADC_JSQR_JEXTEN_Msk, jexten);
    return ok;
}

bool adc_init(void)
{
    /* 校准须在 ADC 使能之前、上电一次即可；规则组与注入组共用一次校准 */
    if (HAL_ADCEx_Calibration_Start(&hadc1, ADC_SINGLE_ENDED) != HAL_OK)
    {
        return false;
    }
    if (HAL_ADCEx_Calibration_Start(&hadc2, ADC_SINGLE_ENDED) != HAL_OK)
    {
        return false;
    }
    return true;
}

bool adc_vbus_read(float *vbus_v, uint32_t *vdda_mv)
{
    uint16_t raw[4];

    if (!adc_reg_read(raw))
    {
        return false;
    }

    uint16_t vrefint = raw[3];
    if (vrefint == 0U) /* 除零兜底：读 0 只可能是采样链路异常 */
    {
        return false;
    }

    /* VDDA_real = 3.000 × VREF_CAL / VREF_meas；加一半分母四舍五入到 mV */
    uint32_t cal  = (uint32_t)*ADC_VREFINT_CAL_ADDR;
    uint32_t vdda = (ADC_VREFINT_CAL_MV * cal + (uint32_t)vrefint / 2U) / (uint32_t)vrefint;

    *vbus_v = (float)raw[0] * ((float)vdda / 1000.0f) / 4095.0f
              * ((ADC_VBUS_DIV_HIGH_KOHM + ADC_VBUS_DIV_LOW_KOHM) / ADC_VBUS_DIV_LOW_KOHM);

    if (vdda_mv != NULL)
    {
        *vdda_mv = vdda;
    }
    return true;
}

bool adc_phase_offset_read(uint16_t raw[ADC_SO_COUNT])
{
    bool ok1 = adc_inj_read(&hadc1, &raw[ADC_SO_M1_IA], &raw[ADC_SO_M1_IB]);
    bool ok2 = adc_inj_read(&hadc2, &raw[ADC_SO_M2_IA], &raw[ADC_SO_M2_IB]);

    return ok1 && ok2;
}

bool adc_dc_cal(uint16_t raw[ADC_SO_COUNT])
{
    drv8301_dc_cal(true);
    /* 建立时间是 µs 级（放大器输出被强制 REF/2），1 ms 保守且代价可忽略 */
    HAL_Delay(1U);

    bool ok1 = adc_inj_read(&hadc1, &raw[ADC_SO_M1_IA], &raw[ADC_SO_M1_IB]);
    bool ok2 = adc_inj_read(&hadc2, &raw[ADC_SO_M2_IA], &raw[ADC_SO_M2_IB]);

    /* 无论读取成败都必须释放，否则电流采样一直失真 */
    drv8301_dc_cal(false);

    return ok1 && ok2;
}
