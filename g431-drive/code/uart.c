#include "headline.h"

/**
  * @file    uart.c
  * @brief   USART2 调试串口（921600）：TX 环形队列 + DMA 链式发送，
  *          RX Circular DMA + IDLE 收包，错误自愈（配置见参数 3.2）
  * @author  sdadz-luo
  */

/* ---- 尺寸（预算依据见参数与配置 3.2） ---- */
#define UART_TX_BUF_SIZE   2048U /* ~57 帧 ≈ 23 ms 突发余量 */
#define UART_LINE_SIZE     128U  /* printf 单行上限，超长行截断发送 */
#define UART_RX_DMA_SIZE   256U   /* 调参命令低频小包 */
#define UART_RX_CMD_SIZE   128U   /* 命令环形队列 */

/* ---- TX 环形队列 ----
   head 只被写方推进，tail 只被发送完成回调推进，busy 标记发送链
   是否在跑——三者跨上下文共享（主循环 + USART2 中断），volatile；
   环形保留一格不满写，head == tail 即空 */
static uint8_t  tx_buf[UART_TX_BUF_SIZE];
static volatile uint16_t tx_head;
static volatile uint16_t tx_tail;
static volatile bool    tx_busy;
static volatile uint16_t tx_dma_len; /* 在途 DMA 段长，TxCplt 据此推进 tail */

/* printf 攒行缓冲：只在主循环上下文使用（约定 printf 不进 ISR），
   无并发保护 */
static uint8_t  line_buf[UART_LINE_SIZE];
static uint16_t line_len;

/* ---- RX ----
   DMA 直写环形缓冲；命令队列由 RxEvent 回调写入、uart_rx_read 消费 */
static uint8_t  rx_dma_buf[UART_RX_DMA_SIZE];
static uint8_t  rx_cmd_buf[UART_RX_CMD_SIZE];
static volatile uint16_t rx_head; /* 写：RxEvent 回调（中断上下文） */
static volatile uint16_t rx_tail; /* 读：主循环解析层 */
static uint16_t  rx_last_pos;     /* 上次 RxEvent 的缓冲位置，增量提取用 */

/* 波形帧静态缓冲：send_wave 的组帧与入队同处一个临界区，
   多上下文同时调用也不会组帧中途被打断产生混帧 */
static uint8_t wave_frame[UART_WAVE_CH_MAX * 4U + 4U];

/* ---- 内部工具（调用方须已持临界区） ---- */

/* TX 队列剩余可写字节数（保留一格防满重叠） */
static uint16_t tx_space(void)
{
    return (uint16_t)((tx_tail + UART_TX_BUF_SIZE - 1U - tx_head) %
                     UART_TX_BUF_SIZE);
}

/* 入队原语：满则丢整段返回 false，绝不部分写（碎帧比丢帧糟） */
static bool tx_enqueue(const uint8_t *data, uint16_t len)
{
    if (len > tx_space())
    {
        return false;
    }
    for (uint16_t i = 0U; i < len; i++)
    {
        tx_buf[tx_head] = data[i];
        tx_head = (uint16_t)((tx_head + 1U) % UART_TX_BUF_SIZE);
    }
    return true;
}

/* 尝试启动发送链：空闲且队列非空时，从 tail（DMA 读端）取顺时针
   到 head 的数据段，到缓冲末尾截断，交给 DMA；剩余部分由 TxCplt
   链式续发。写方只推进 head，tail 只由完成回调推进——本函数不
   动队列索引，失败重试也不产生错位 */
static void tx_try_start(void)
{
    uint16_t len;

    if (tx_busy || (tx_head == tx_tail))
    {
        return;
    }

    /* 有效数据从 tail 顺时针走到 head；单次 DMA 段不得越过缓冲末尾 */
    len = (uint16_t)((tx_head + UART_TX_BUF_SIZE - tx_tail) % UART_TX_BUF_SIZE);
    if (len > (uint16_t)(UART_TX_BUF_SIZE - tx_tail))
    {
        len = (uint16_t)(UART_TX_BUF_SIZE - tx_tail);
    }

    if (HAL_UART_Transmit_DMA(&huart2, &tx_buf[tx_tail], len) == HAL_OK)
    {
        tx_dma_len = len;
        tx_busy = true;
    }
}

/* ---- 对外接口 ---- */

void uart_init(void)
{
    /* app_init 序列调用。CubeMX 已完成串口与 DMA 通道配置，这里只
       复位队列状态并启动接收链；发送链首次写入时自行启动 */
    tx_head = 0U;
    tx_tail = 0U;
    tx_busy = false;
    tx_dma_len = 0U;
    line_len = 0U;
    rx_head = 0U;
    rx_tail = 0U;
    rx_last_pos = 0U;

    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_dma_buf, UART_RX_DMA_SIZE);

    /* 链路自检：串口助手看到此行即「入队 → DMA → 921600」全链已通 */
    (void)uart_send_bytes((const uint8_t *)"cygnus g431 drive ready\r\n", 25U);
}

bool uart_send_bytes(const uint8_t *data, uint16_t len)
{
    bool ok = false;

    if ((data == NULL) || (len == 0U))
    {
        return false;
    }

    /* 临界区为亚 µs 级（几十字节拷贝 + 状态机推进），对 20 kHz
       环路无感；入队即返回，永不阻塞调用者 */
    __disable_irq();
    ok = tx_enqueue(data, len);
    if (ok)
    {
        tx_try_start();
    }
    __enable_irq();

    return ok;
}

bool uart_send_wave(const float *channels, uint8_t count)
{
    bool ok = false;
    uint16_t len;

    if ((channels == NULL) || (count == 0U) || (count > UART_WAVE_CH_MAX))
    {
        return false;
    }

    /* 组帧与入队同临界区：wave_frame 为静态缓冲，防止组帧中途被打
       断后入队的是两个上下文混写的帧 */
    __disable_irq();
    len = vofa_pack_frame(channels, count, wave_frame);
    ok = tx_enqueue(wave_frame, len);
    if (ok)
    {
        tx_try_start();
    }
    __enable_irq();

    return ok;
}

/* ---- ISR 转调（运行上下文：USART2 中断，优先级 4） ---- */

void uart_on_tx_complete(void)
{
    __disable_irq();
    tx_busy = false;
    tx_tail = (uint16_t)((tx_tail + tx_dma_len) % UART_TX_BUF_SIZE);
    tx_try_start(); /* 队列还有剩就接着发，DMA 不空转 */
    __enable_irq();
}

void uart_on_rx_event(uint16_t pos)
{
    /* HAL circular 模式下 pos 是缓冲内当前位置而非增量，且全满事件
     * 会传来 Size（= 缓冲长度），折回 0 统一处理 */
    pos %= UART_RX_DMA_SIZE;

    __disable_irq();
    while (rx_last_pos != pos)
    {
        rx_cmd_buf[rx_head] = rx_dma_buf[rx_last_pos];
        rx_head = (uint16_t)((rx_head + 1U) % UART_RX_CMD_SIZE);
        if (rx_head == rx_tail)
        {
            /* 命令队列满丢最旧——调参命令最新的最重要 */
            rx_tail = (uint16_t)((rx_tail + 1U) % UART_RX_CMD_SIZE);
        }
        rx_last_pos = (uint16_t)((rx_last_pos + 1U) % UART_RX_DMA_SIZE);
    }
    __enable_irq();
}

void uart_on_error(void)
{
    /* CubeMX 已使能 DMA-on-RX-Error（错误时 DMA 不停），但 HAL 的
     * 错误路径仍会 abort 接收——这里整体重启一次保证接收链永远
     * 活着；若接收本就正常，重复启动被 HAL 拒绝，幂等无害 */
    __disable_irq();
    (void)HAL_UARTEx_ReceiveToIdle_DMA(&huart2, rx_dma_buf, UART_RX_DMA_SIZE);
    __enable_irq();
}

/* ---- 命令取出（主循环上下文） ---- */

uint16_t uart_rx_read(uint8_t *out, uint16_t max)
{
    uint16_t count = 0U;

    if (out == NULL)
    {
        return 0U;
    }

    __disable_irq();
    while ((count < max) && (rx_tail != rx_head))
    {
        out[count] = rx_cmd_buf[rx_tail];
        rx_tail = (uint16_t)((rx_tail + 1U) % UART_RX_CMD_SIZE);
        count++;
    }
    __enable_irq();

    return count;
}

/* ---- printf 重定向（仅主循环上下文，行尾须带 \n） ---- */

int fputc(int ch, FILE *stream)
{
    (void)stream;

    /* 行缓冲将满时先截断补换行发送，给后续字符留位 */
    if (line_len >= (UART_LINE_SIZE - 1U))
    {
        line_buf[line_len] = (uint8_t)'\n';
        (void)uart_send_bytes(line_buf, (uint16_t)(line_len + 1U));
        line_len = 0U;
    }

    line_buf[line_len] = (uint8_t)ch;
    line_len++;

    if (ch == '\n')
    {
        (void)uart_send_bytes(line_buf, line_len);
        line_len = 0U;
    }

    return ch;
}
