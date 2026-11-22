#include "headline.h"

/**
  * @file    protect.c
  * @brief   保护类逻辑集中地（STM32G431RB，HAL G4）
  * @author  sdadz-luo
  */

void protect_init(void)
{
    /* 调试外设冻结（freeze 位，设计方案 3.2 第 5 条）：CPU 被调试器
     * 暂停（断点/单步）时下列外设时钟同步停止。只在调试事件时生效，
     * 正常运行无影响。
     *
     * 必须在固件代码里配置、不依赖 MDK 的 dbgconf——DebugConfig/
     * 不入库（.gitignore），队友克隆后该配置不存在，断点下功率级
     * 仍会换相（设计方案 3.2 第 5 条原文警告）
     *
     * 注意 freeze 只保证不再换相，输出保持暂停瞬间的电平而非安全
     * 态——带功率调试时先关功率级（EN_GATE / MOE / 母线）再下断点 */
    SET_BIT(DBGMCU->APB2FZ, DBGMCU_APB2FZ_DBG_TIM1_STOP);  /* 电机 1 PWM */
    SET_BIT(DBGMCU->APB2FZ, DBGMCU_APB2FZ_DBG_TIM8_STOP);  /* 电机 2 PWM */
    SET_BIT(DBGMCU->APB1FZR1, DBGMCU_APB1FZR1_DBG_TIM4_STOP); /* 泄放回路 */
    SET_BIT(DBGMCU->APB1FZR1, DBGMCU_APB1FZR1_DBG_IWDG_STOP); /* 单步不咬狗 */
}
