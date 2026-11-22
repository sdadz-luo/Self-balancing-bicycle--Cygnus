#include "headline.h"

/**
  * @file    isr.c
  * @brief   中断服务集中地：中断入口函数由 CubeMX 生成在
  *          Core/Src/stm32g4xx_it.c，本文件承载 HAL 回调实现
  *          （同样运行在中断上下文），逻辑转调各模块的 on_xxx
  * @author  sdadz-luo
  */

/* USART2 发送完成：链式续发，逻辑在 uart.c */
void HAL_UART_TxCpltCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        uart_on_tx_complete();
    }
}

/* USART2 接收事件（IDLE / 半满）：命令字节入队 */
void HAL_UARTEx_RxEventCallback(UART_HandleTypeDef *huart, uint16_t Size)
{
    if (huart->Instance == USART2)
    {
        uart_on_rx_event(Size);
    }
}

/* USART2 错误：重启接收链，接收永不静默死亡 */
void HAL_UART_ErrorCallback(UART_HandleTypeDef *huart)
{
    if (huart->Instance == USART2)
    {
        uart_on_error();
    }
}
