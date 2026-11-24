#ifndef SPI_H
#define SPI_H

#include <stdbool.h>
#include <stdint.h>

/* 总线编号与电机通道一一对应（设计方案 1.2）：
   BUS1 = SPI1 = DRV8301-1 + MT6816-1，BUS2 = SPI3 = DRV8301-2 + MT6816-2 */
typedef enum
{
    SPI_BUS_1 = 0,
    SPI_BUS_2,
    SPI_BUS_COUNT
} SpiBus;

/* 片选全部拉高、两条总线置编码器常态档（Mode 3 / /32） */
void spi_bus_init(void);

/* 以 DRV8301 档（Mode 1 / /64）收发一个 16 位帧，结束后切回编码器档。
   16 位帧内 CS 保持低；rx 可为 NULL。
   与编码器事务共用总线：调用方须保证此刻没有编码器读取在进行
   （设计方案 3.2 第 11 条，编码器读取落地后按该条加仲裁） */
bool spi_drv_xfer(SpiBus bus, uint16_t tx, uint16_t *rx);

#endif /* SPI_H */
