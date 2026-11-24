#include "headline.h"

/**
  * @file    drv8301.c
  * @brief   DRV8301 寄存器层：16 位 SPI 帧（R/W + 4 位地址 + 11 位数据）、
  *          上电配置与读回校验、故障状态解析、GATE_RESET 故障清除
  * @author  sdadz-luo
  */

/* 控制引脚（设计方案 1.1 / 2.6）：EN_GATE 推挽 + 外部下拉、高电平使能；
   nFAULT 开漏 + 外部上拉；DC_CAL 两片共用 */
#define DRV_EN_GATE_PORT GPIOB
#define DRV_EN_GATE_PIN  GPIO_PIN_2
#define DRV_NFAULT1_PORT GPIOA
#define DRV_NFAULT1_PIN  GPIO_PIN_15
#define DRV_NFAULT2_PORT GPIOD
#define DRV_NFAULT2_PIN  GPIO_PIN_2
#define DRV_DC_CAL_PORT  GPIOB
#define DRV_DC_CAL_PIN   GPIO_PIN_10

/* 帧格式（SLOS719F 7.5.2 / Table 6）：W15 = R/W，A[14:11] 地址，D[10:0] 数据；
   SDO 响应帧（Table 7）的 F[15] 为帧错误位、A[14:11] 回显地址 */
#define DRV_CMD_READ  0x8000U
#define DRV_CMD_WRITE 0x0000U
#define DRV_ADDR_SHIFT 11U
#define DRV_DATA_MASK  0x07FFU
#define DRV_RESP_FRAME_FAULT 0x8000U

/* CTRL1（SLOS719F Table 11）。位序以实测读回为准——PDF 版 Table 11 的列
   标注与芯片实际行为矛盾：写 0x03E 读回 0x03A，唯一差异 D2 恰是 GATE_RESET
   写 1 后自清（"reverts to 0"）的行为，证明 GATE_RESET = D2、低位字段自 D0
   起排；Table 13 标题「Control Bit (D6–D10)」与 SimpleFOC/motorloop 实现
   一致，OC_ADJ_SET 在 D10:D6：
     OC_ADJ_SET   = D10:D6（00 限流…见 Table 13）
     OCP_MODE     = D5:D4（00 限流、01 锁存关断、10 仅上报、11 禁用）
     PWM_MODE     = D3（0 = 6-PWM）
     GATE_RESET   = D2（写 1 复位锁存故障，自清）
     GATE_CURRENT = D1:D0（00 = 1.7 A，20 kHz 下平均驱动电流远低于限制）
   OC_ADJ_SET = 30 → VDS 2.131 V。依据：NCE6045G RDS(on)≈10 mΩ@25°C，
   高温按 1.5 倍取 15 mΩ，跳闸目标 150 A → 2.25 V，Table 13 就近取码 30
   （高温约 142 A、25°C 约 213 A）；台架阶段 3 逐级升流实测校正 */
#define DRV_CTRL1_CFG        ((30U << 6) | (1U << 4))
#define DRV_CTRL1_GATE_RESET (1U << 2)

/* CTRL2（SLOS719F Table 12）：全部保持默认值 0（OCTW 报 OT+OC、增益 10 V/V、
   DC_CAL 接负载、逐周期）。
   注意：CTRL2 的位布局本轮未实测确认（PDF 版 Table 12 的列标注同样不可靠），
   将来改 GAIN/OCTW_MODE/DC_CAL 前先用「写 1 读回」法确认位位置，
   勿按 PDF 列标注直接写 */
#define DRV_CTRL2_CFG 0x0000U

#define DRV_INIT_RETRY   3U  /* 配置 + 读回校验的尝试次数（用户拍板） */
#define DRV_SPI_READY_MS 10U /* t_SPI_READY 上限（SLOS719F 6.8：5~10 ms） */

/* EN_GATE 拉高时刻：t_SPI_READY 与 nFAULT 超时都从这里起算 */
static uint32_t en_gate_ms;

static SpiBus dev_bus(DrvDev dev)
{
    return (dev == DRV_DEV_1) ? SPI_BUS_1 : SPI_BUS_2;
}

void drv8301_enable(void)
{
    en_gate_ms = HAL_GetTick();
    HAL_GPIO_WritePin(DRV_EN_GATE_PORT, DRV_EN_GATE_PIN, GPIO_PIN_SET);
}

bool drv8301_wait_ready(uint32_t timeout_ms)
{
    /* 先等满 t_SPI_READY：nFAULT 变高只说明无故障，早于 SPI 就绪 */
    while ((HAL_GetTick() - en_gate_ms) < DRV_SPI_READY_MS)
    {
    }

    while ((HAL_GetTick() - en_gate_ms) < timeout_ms)
    {
        /* nFAULT 开漏 + 外部上拉：EN_GATE 拉高且无故障时才为高。
           引脚配成 AF（BKIN），IDR 在 AF 输入下照样反映电平 */
        if ((HAL_GPIO_ReadPin(DRV_NFAULT1_PORT, DRV_NFAULT1_PIN) == GPIO_PIN_SET) &&
            (HAL_GPIO_ReadPin(DRV_NFAULT2_PORT, DRV_NFAULT2_PIN) == GPIO_PIN_SET))
        {
            return true;
        }
    }
    return false;
}

bool drv8301_write(DrvDev dev, uint8_t reg, uint16_t val)
{
    uint16_t cmd = DRV_CMD_WRITE | ((uint16_t)(reg & 0x0FU) << DRV_ADDR_SHIFT) |
                   (val & DRV_DATA_MASK);

    /* 写帧的 SDO 响应是 Status1，配置阶段不需要 */
    if (!spi_drv_xfer(dev_bus(dev), cmd, NULL))
    {
        printf("drv%u wr 0x%02X fail\r\n", (unsigned int)dev + 1U, (unsigned int)reg);
        return false;
    }
    return true;
}

bool drv8301_read(DrvDev dev, uint8_t reg, uint16_t *val)
{
    uint16_t cmd = DRV_CMD_READ | ((uint16_t)(reg & 0x0FU) << DRV_ADDR_SHIFT);
    uint16_t resp;

    /* 读要两帧：第 N 帧发命令、第 N+1 帧 SDO 才返回该地址数据（7.5.1） */
    if (!spi_drv_xfer(dev_bus(dev), cmd, NULL))
    {
        printf("drv%u rd 0x%02X xfer fail\r\n", (unsigned int)dev + 1U, (unsigned int)reg);
        return false;
    }
    if (!spi_drv_xfer(dev_bus(dev), cmd, &resp))
    {
        printf("drv%u rd 0x%02X xfer fail\r\n", (unsigned int)dev + 1U, (unsigned int)reg);
        return false;
    }

    /* F[15]=1 表示帧无效；地址回显必须一致 */
    if (((resp & DRV_RESP_FRAME_FAULT) != 0U) ||
        (((resp >> DRV_ADDR_SHIFT) & 0x0FU) != (reg & 0x0FU)))
    {
        printf("drv%u rd 0x%02X resp=0x%04X\r\n", (unsigned int)dev + 1U,
               (unsigned int)reg, (unsigned int)resp);
        return false;
    }
    if (val != NULL)
    {
        *val = resp & DRV_DATA_MASK;
    }
    return true;
}

bool drv8301_init(DrvDev dev)
{
    for (uint32_t attempt = 0U; attempt < DRV_INIT_RETRY; attempt++)
    {
        uint16_t rb1 = 0U;
        uint16_t rb2 = 0U;

        if (!drv8301_write(dev, DRV_REG_CTRL1, DRV_CTRL1_CFG) ||
            !drv8301_write(dev, DRV_REG_CTRL2, DRV_CTRL2_CFG))
        {
            continue;
        }
        if (!drv8301_read(dev, DRV_REG_CTRL1, &rb1) || !drv8301_read(dev, DRV_REG_CTRL2, &rb2))
        {
            continue;
        }
        if ((rb1 != DRV_CTRL1_CFG) || (rb2 != DRV_CTRL2_CFG))
        {
            printf("drv%u rb c1=0x%03X/0x%03X c2=0x%03X/0x%03X\r\n",
                   (unsigned int)dev + 1U, (unsigned int)rb1, (unsigned int)DRV_CTRL1_CFG,
                   (unsigned int)rb2, (unsigned int)DRV_CTRL2_CFG);
            continue;
        }
        return true;
    }
    printf("drv%u init fail\r\n", (unsigned int)dev + 1U);
    return false;
}

bool drv8301_status(DrvDev dev, DrvStatus *st)
{
    uint16_t s1 = 0U;
    uint16_t s2 = 0U;

    if ((st == NULL) || !drv8301_read(dev, DRV_REG_STATUS1, &s1) ||
        !drv8301_read(dev, DRV_REG_STATUS2, &s2))
    {
        return false;
    }
    st->status1 = s1;
    st->status2 = s2;
    return true;
}

bool drv8301_fault_clear(DrvDev dev)
{
    return drv8301_write(dev, DRV_REG_CTRL1, DRV_CTRL1_CFG | DRV_CTRL1_GATE_RESET);
}

void drv8301_dc_cal(bool on)
{
    HAL_GPIO_WritePin(DRV_DC_CAL_PORT, DRV_DC_CAL_PIN, on ? GPIO_PIN_SET : GPIO_PIN_RESET);
}
