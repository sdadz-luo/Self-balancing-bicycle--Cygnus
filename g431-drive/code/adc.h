#ifndef ADC_H
#define ADC_H

#include <stdbool.h>
#include <stdint.h>

/* 四路分流放大器输出，与电机通道一一对应 */
typedef enum
{
    ADC_SO_M1_IA = 0,
    ADC_SO_M1_IB,
    ADC_SO_M2_IA,
    ADC_SO_M2_IB,
    ADC_SO_COUNT
} AdcSo;

/* 上电校准：两路各做一次，须在 ADC 使能之前（规则组与注入组共用一次校准） */
bool adc_init(void);

/* 规则组一次转换：VBUS → NTC1 → NTC2 → VREFINT（按 rank 顺序），
   由 VREFINT 出厂校准值反推实际 VDDA 再换算电压。vdda_mv 可为 NULL。
   NTC 两路在 V1.0 板上未接（原理图 NC），转换照做但不作判据 */
bool adc_vbus_read(float *vbus_v, uint32_t *vdda_mv);

/* 软件触发一次注入组，读四路 SO 原始码（顺序见 AdcSo）。
   期望 ≈2048（REF/2）；须在 EN_GATE 拉高、PWM 未开时调用 */
bool adc_phase_offset_read(uint16_t raw[ADC_SO_COUNT]);

/* DC_CAL（PB10）拉高 → 等建立 → 读四路 → 拉低（无论成败都拉低）。
   此时 SO 被强制为 REF/2，期望 ≈2048 */
bool adc_dc_cal(uint16_t raw[ADC_SO_COUNT]);

#endif /* ADC_H */
