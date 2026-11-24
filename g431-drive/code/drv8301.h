#ifndef DRV8301_H
#define DRV8301_H

#include <stdbool.h>
#include <stdint.h>

/* 两片 DRV8301 与电机通道一一对应（设计方案 1.1 / 1.2）：
   DRV1：SPI1、nSCS1=PC4、nFAULT1=PA15、nOCTW1=PC13
   DRV2：SPI3、nSCS2=PB6、nFAULT2=PD2、 nOCTW2=PA4 */
typedef enum
{
    DRV_DEV_1 = 0,
    DRV_DEV_2,
    DRV_DEV_COUNT
} DrvDev;

/* 寄存器地址（SLOS719F Table 8） */
#define DRV_REG_STATUS1 0x00U /* 只读：故障状态（OC 位读后自清） */
#define DRV_REG_STATUS2 0x01U /* 只读：GVDD_OV 与 Device ID */
#define DRV_REG_CTRL1   0x02U /* 读写：栅极驱动控制 */
#define DRV_REG_CTRL2   0x03U /* 读写：采样放大器与杂项控制 */

/* Status1 位（SLOS719F Table 9） */
#define DRV_ST1_FAULT   (1U << 10)
#define DRV_ST1_GVDD_UV (1U << 9)
#define DRV_ST1_PVDD_UV (1U << 8)
#define DRV_ST1_OTSD    (1U << 7)
#define DRV_ST1_OTW     (1U << 6)
#define DRV_ST1_OC_MASK 0x003FU /* FETHA/FETLA/FETHB/FETLB/FETHC/FETLC */

/* Status2 位（SLOS719F Table 10） */
#define DRV_ST2_GVDD_OV     (1U << 7)
#define DRV_ST2_DEV_ID_MASK 0x000FU

/* 故障状态快照：OC 状态位读后自清，读到即存入本结构（设计方案 3.2 第 8 条） */
typedef struct
{
    uint16_t status1;
    uint16_t status2;
} DrvStatus;

/* EN_GATE（PB2）拉高：两片共用，同时使能栅极驱动与采样放大器。
   必须在 SPI 访问之前——EN_GATE 低电平期间 SPI 不可用（内部稳压器休眠） */
void drv8301_enable(void);

/* 轮询两路 nFAULT 均释放（AF 输入下直读 IDR），超时返回 false。
   EN_GATE 拉高后芯片约 5~10 ms SPI 才就绪（SLOS719F 6.8 t_SPI_READY） */
bool drv8301_wait_ready(uint32_t timeout_ms);

/* 单片配置（CTRL1/CTRL2 写入 + 读回比对），失败重试 3 次。
   前置条件：EN_GATE 已拉高、nFAULT 已释放 */
bool drv8301_init(DrvDev dev);

bool drv8301_read(DrvDev dev, uint8_t reg, uint16_t *val);
bool drv8301_write(DrvDev dev, uint8_t reg, uint16_t val);

/* 读 Status1/Status2 快照（OC 位读后自清，务必使用本函数一次取全） */
bool drv8301_status(DrvDev dev, DrvStatus *st);

/* SPI GATE_RESET 清栅极驱动锁存故障（不扰动配置、绕开 EN_GATE 10~20 µs
   失联禁区；GVDD_OV 例外——只能完整 EN_GATE 复位，SLOS719F 7.4.1） */
bool drv8301_fault_clear(DrvDev dev);

/* 两片共用的 DC_CAL 引脚（PB10）：高电平短接采样放大器输入读零点 */
void drv8301_dc_cal(bool on);

#endif /* DRV8301_H */
