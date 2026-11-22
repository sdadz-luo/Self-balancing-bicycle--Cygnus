#include "headline.h"

/**
  * @file    uart.c
  * @brief   USART2 调试串口（PA2/PA3）：printf 重定向与整串阻塞发送
  * @author  sdadz-luo
  */

/* 超时只防 UART 外设异常时死等：115200 波特率一字节约 87 us，
   10 ms / 100 ms 已覆盖单字符与整行日志，正常发送远不会触及 */
#define UART_TX_BYTE_TIMEOUT_MS  10U
#define UART_TX_LINE_TIMEOUT_MS  100U

/* printf 重定向：MDK C 库的字符输出口（MicroLIB / 标准库均经此走 huart2）。
   发送失败只能丢弃——调试口无重试路径，故显式忽略返回值 */
int fputc(int ch, FILE *stream)
{
    (void)stream;
    (void)HAL_UART_Transmit(&huart2, (const uint8_t *)&ch, 1U,
                            UART_TX_BYTE_TIMEOUT_MS);
    return ch;
}

/* 整串阻塞发送：一次 HAL 调用发完整段，比 printf 逐字符过 fputc 省函数
   调用与状态机开销，调试日志的主出口 */
void uart_send_bytes(const uint8_t *data, uint16_t len)
{
    if ((data == NULL) || (len == 0U))
    {
        return;
    }
    (void)HAL_UART_Transmit(&huart2, data, len, UART_TX_LINE_TIMEOUT_MS);
}
