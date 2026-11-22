#ifndef UART_H
#define UART_H

#include <stdbool.h> /* bool 类型来源 */
#include <stdint.h>  /* uint8_t / uint16_t 类型来源 */

/**
  * @file    uart.h
  * @brief   USART2 调试串口（PA2/PA3，921600）：TX 环形队列 + DMA
  *          链式发送，RX Circular DMA + IDLE 收包，错误自愈。
  *          串口本体初始化走 CubeMX 的 MX_USART2_UART_Init()，
  *          uart_init 只做 DMA 启动与状态复位
  * @author  sdadz-luo
  */

/* 波形帧最大通道数：uart_send_wave 的静态组帧缓冲按此封顶（8 路
   ×4B + 4B 帧尾 = 36B/帧），防越界 */
#define UART_WAVE_CH_MAX  8U

void uart_init(void);

/* 普通格式发送（文本/日志/调试字节）：入 TX 环形队列，永不阻塞；
   队列满丢整段返回 false。ISR 与主循环上下文均可调用 */
bool uart_send_bytes(const uint8_t *data, uint16_t len);

/* 波形数据发送：内部经 vofa_pack_frame 组 JustFloat 帧后入队，
   count 须 ≤ UART_WAVE_CH_MAX；同样满丢整帧、永不阻塞 */
bool uart_send_wave(const float *channels, uint8_t count);

/* 以下三个由 isr.c 的 HAL 回调转调，勿在别处调用 */
void uart_on_tx_complete(void);          /* TxCplt：链式续发 */
void uart_on_rx_event(uint16_t pos);      /* RxEvent：命令字节入队 */
void uart_on_error(void);                 /* Error：重启接收链 */

/* 解析层取命令字节（协议未定稿，先备好接口），返回实际取出长度 */
uint16_t uart_rx_read(uint8_t *out, uint16_t max);

#endif /* UART_H */
