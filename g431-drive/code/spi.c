#include "headline.h"

/* 句柄定义在 CubeMX 生成的 Core/Src/spi.c；与 Core/Inc/spi.h 同名、
   按名 include 无法命中那一份，在唯一使用处直接声明 */
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi3;

/**
  * @file    spi.c
  * @brief   SPI 总线层：每条总线挂一片 DRV8301（Mode 1）与一片 MT6816
  *          （Mode 3），按设计方案 1.2 在运行时切换 CR1 的 CPOL/CPHA/分频
  * @author  sdadz-luo
  */

/* 片选（设计方案 1.2 / 2.6）：软件 NSS、低有效。复位后 MX_GPIO_Init 已置高，
   本模块初始化时再兜底一次，此后四路电平由总线层负责 */
#define SPI_NSCS1_PORT   GPIOC
#define SPI_NSCS1_PIN    GPIO_PIN_4
#define SPI_NSCS2_PORT   GPIOB
#define SPI_NSCS2_PIN    GPIO_PIN_6
#define SPI_ENC1_CS_PORT GPIOC
#define SPI_ENC1_CS_PIN  GPIO_PIN_5
#define SPI_ENC2_CS_PORT GPIOB
#define SPI_ENC2_CS_PIN  GPIO_PIN_7

/* CR1 中随从机切换的字段。SPI 内核时钟 170 MHz：
   /32 ≈ 5.31 MHz —— 编码器档（MT6816 上限 10 MHz）
   /64 ≈ 2.66 MHz —— DRV8301 档（SLOS719F 6.8：t_CLK ≥ 100 ns，上限 10 MHz；
                    取 1~5 MHz 设计区间内的保守档） */
#define SPI_CR1_SWITCH_MASK (SPI_CR1_CPOL | SPI_CR1_CPHA | SPI_CR1_BR_Msk)
#define SPI_CR1_MODE_ENC    (SPI_CR1_CPOL | SPI_CR1_CPHA | SPI_CR1_BR_2)
#define SPI_CR1_MODE_DRV    (SPI_CR1_CPHA | SPI_CR1_BR_2 | SPI_CR1_BR_0)

/* 单字节 @2.66 MHz ≈ 3 µs；1 ms 已含两个数量级裕量，超时即判总线异常 */
#define SPI_XFER_TIMEOUT_MS 1U

/* BSY 等待上限：正常传输远小于此数；死等会让故障现场失去上报机会 */
#define SPI_BSY_GUARD 100000U

static SPI_HandleTypeDef *spi_handle(SpiBus bus)
{
    return (bus == SPI_BUS_1) ? &hspi1 : &hspi3;
}

static void spi_cs_drv(SpiBus bus, GPIO_PinState level)
{
    if (bus == SPI_BUS_1)
    {
        HAL_GPIO_WritePin(SPI_NSCS1_PORT, SPI_NSCS1_PIN, level);
    }
    else
    {
        HAL_GPIO_WritePin(SPI_NSCS2_PORT, SPI_NSCS2_PIN, level);
    }
}

/* 切换档位：等 BSY 清零 → SPE=0 → 改 CR1 → SPE=1（设计方案 1.2）。
   调用方保证两片 CS 都为高：DRV8301 要求 nSCS 拉低时 SCLK 为低，
   切换只在 CS 高期间发生，SCLK 静止电平的跳变不会落进任何帧 */
static bool spi_set_mode(SpiBus bus, uint32_t mode_bits)
{
    SPI_TypeDef *spi   = spi_handle(bus)->Instance;
    uint32_t     guard = SPI_BSY_GUARD;

    while (((spi->SR & SPI_SR_BSY) != 0U) && (guard > 0U))
    {
        guard--;
    }
    if (guard == 0U)
    {
        return false;
    }
    CLEAR_BIT(spi->CR1, SPI_CR1_SPE);
    MODIFY_REG(spi->CR1, SPI_CR1_SWITCH_MASK, mode_bits);
    SET_BIT(spi->CR1, SPI_CR1_SPE);
    return true;
}

void spi_bus_init(void)
{
    /* 四路片选兜底拉高：不依赖生成代码的初始电平细节（.ioc 改动可能带走它） */
    HAL_GPIO_WritePin(SPI_NSCS1_PORT, SPI_NSCS1_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SPI_NSCS2_PORT, SPI_NSCS2_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SPI_ENC1_CS_PORT, SPI_ENC1_CS_PIN, GPIO_PIN_SET);
    HAL_GPIO_WritePin(SPI_ENC2_CS_PORT, SPI_ENC2_CS_PIN, GPIO_PIN_SET);

    /* 静态档 = 编码器档（CubeMX 生成配置即 Mode 3 / /32）。显式再写一次，
       防止 .ioc 被改动后总线带着错误档位进入 DRV 访问 */
    (void)spi_set_mode(SPI_BUS_1, SPI_CR1_MODE_ENC);
    (void)spi_set_mode(SPI_BUS_2, SPI_CR1_MODE_ENC);
}

bool spi_drv_xfer(SpiBus bus, uint16_t tx, uint16_t *rx)
{
    SPI_HandleTypeDef *h     = spi_handle(bus);
    uint8_t            tx_b;
    uint8_t            rx_hi = 0U;
    uint8_t            rx_lo = 0U;
    bool               ok;

    if (!spi_set_mode(bus, SPI_CR1_MODE_DRV))
    {
        return false;
    }

    /* 16 位帧拆成两次单字节调用：HAL 在 Size>1 时会按 uint16 访问缓冲区，
       对齐与字节序依赖实现细节；单字节路径无歧义。DRV8301 允许帧内任意
       间隙（SLOS719F 7.5.1），CS 全程保持低 */
    spi_cs_drv(bus, GPIO_PIN_RESET);
    tx_b = (uint8_t)(tx >> 8); /* MSB first（SLOS719F Table 6） */
    ok   = (HAL_SPI_TransmitReceive(h, &tx_b, &rx_hi, 1U, SPI_XFER_TIMEOUT_MS) == HAL_OK);
    if (ok)
    {
        tx_b = (uint8_t)tx;
        ok   = (HAL_SPI_TransmitReceive(h, &tx_b, &rx_lo, 1U, SPI_XFER_TIMEOUT_MS) == HAL_OK);
    }
    spi_cs_drv(bus, GPIO_PIN_SET);

    /* 无论成败都切回编码器档：失败后总线也必须回到常态，否则编码器侧
       会静默拿到 Mode 1 时钟（下一阶段的角度数据即错） */
    (void)spi_set_mode(bus, SPI_CR1_MODE_ENC);

    if (ok && (rx != NULL))
    {
        *rx = ((uint16_t)rx_hi << 8) | (uint16_t)rx_lo;
    }
    return ok;
}
