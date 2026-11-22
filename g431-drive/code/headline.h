#ifndef HEADLINE_H
#define HEADLINE_H

/**
  * @file    headline.h
  * @brief   统一 include 入口：HAL、标准库与全部模块头在此聚合，
  *          各 .c 只 include 本头文件（约定见端级 CLAUDE.md）
  * @author  sdadz-luo
  */

/* ---- 常用基础头 ---- */
#include "stm32g4xx_hal.h" /* HAL 主头：CMSIS 设备头 + 全部 HAL 外设驱动 */ // IWYU pragma: export
#include <stdint.h>        /* uint8_t / int32_t / UINT16_MAX 等固定宽度类型 */ // IWYU pragma: export
#include <stdbool.h>       /* bool / true / false */ // IWYU pragma: export
#include <stddef.h>        /* size_t / NULL / offsetof，缓冲区操作与 CAN 帧打包 */ // IWYU pragma: export
#include <string.h>        /* memcpy / memset，组帧与结构体清零 */ // IWYU pragma: export
#include <float.h>         /* FLT_MAX / FLT_EPSILON，环限幅默认值与浮点容差 */ // IWYU pragma: export
#include <math.h>          /* sqrtf / atan2f / sinf / cosf，FOC 与 PID 运算 */ // IWYU pragma: export
#include <stdio.h>         /* printf，经调试串口（PA2/PA3）fputc 重定向输出 */ // IWYU pragma: export

/* ---- CubeMX 生成外设头（取 HAL 实例句柄的 extern 声明） ---- */
#include "tim.h"     /* htim1 / htim4 / htim8：双电机 PWM 与母线泄放 */ // IWYU pragma: export
#include "adc.h"     /* hadc1 / hadc2：相电流采样 */ // IWYU pragma: export
#include "fdcan.h"   /* hfdcan1：CAN 通信 */ // IWYU pragma: export
#include "usart.h"   /* huart2：调试串口 */ // IWYU pragma: export
#include "iwdg.h"    /* hiwdg：窗口看门狗 */ // IWYU pragma: export
/* gpio.h 不列：无句柄，GPIO 操作的 API 与宏全在 HAL 主头；
   stm32g4xx_it.h 不列：中断函数不需要原型对外发布；
   Core/Inc/spi.h 不列：与 code/spi.h 同名、按名 include 永远命中
   同目录这份，hspi1 / hspi3 的 extern 改在 code/spi.h 里直接声明 */

/* ---- 驱动层 ---- */
#include "spi.h"           /* SPI 总线（双 DRV8301 + 双 MT6816 片选管理） */ // IWYU pragma: export
#include "drv8301.h"       /* 栅极驱动寄存器配置与故障读取 */ // IWYU pragma: export
#include "mt6816.h"        /* 磁编码器角度读取 */ // IWYU pragma: export
#include "uart.h"         /* 调试串口（USART2，PA2/PA3）：printf 重定向 + TX 环形队列 + DMA 链式发送 */ // IWYU pragma: export
#include "vofa.h"         /* VOFA JustFloat 组帧（调试波形通道，经 uart 发送） */ // IWYU pragma: export

/* ---- 算法层 ---- */
#include "pid.h"           /* PID 控制器 */ // IWYU pragma: export
#include "svpwm.h"         /* SVPWM 调制 */ // IWYU pragma: export
#include "foc.h"           /* FOC 核心算法 */ // IWYU pragma: export

/* ---- 系统层 ---- */
#include "motor.h"         /* 双电机对象（动量轮力矩环 + 摩擦驱动速度环） */ // IWYU pragma: export
#include "brake_chopper.h" /* 母线能量耗散回路 */ // IWYU pragma: export
#include "can.h"           /* FDCAN 三端契约端点 */ // IWYU pragma: export
#include "isr.h"           /* 中断服务函数集中 */ // IWYU pragma: export

/* ---- 保护与应用层 ---- */
#include "protect.h"       /* freeze 位配置 + CSS 受控停机 */ // IWYU pragma: export
#include "app.h"           /* 应用初始化序列与主循环 */ // IWYU pragma: export

#endif /* HEADLINE_H */
