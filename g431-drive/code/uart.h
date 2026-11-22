#ifndef UART_H
#define UART_H

#include <stdint.h> /* uint8_t / uint16_t 类型来源 */

/**
  * @file    uart.h
  * @brief   USART2 调试串口（PA2/PA3，见设计方案 1.4 调试口）：printf 重定向
  *          与整串阻塞发送。初始化走 CubeMX 生成的 MX_USART2_UART_Init()，
  *          本模块不重复 init
  * @author  sdadz-luo
  */

void uart_send_bytes(const uint8_t *data, uint16_t len);

#endif /* UART_H */
