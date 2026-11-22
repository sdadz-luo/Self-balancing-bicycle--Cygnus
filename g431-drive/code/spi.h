#ifndef SPI_H
#define SPI_H

#include "stm32g4xx_hal.h" /* SPI_HandleTypeDef 类型来源 */

/* 句柄定义在 CubeMX 生成的 Core/Src/spi.c；因与 Core/Inc/spi.h 同名、
   按名 include 无法命中那一份，这里直接 extern 声明 */
extern SPI_HandleTypeDef hspi1;
extern SPI_HandleTypeDef hspi3;

#endif /* SPI_H */
