#ifndef VOFA_H
#define VOFA_H

#include <stdint.h> /* uint8_t / uint16_t 类型来源 */

/**
  * @file    vofa.h
  * @brief   VOFA JustFloat 协议组帧（纯格式转换，不参与发送）：
  *          n 路 float 小端字节 + 帧尾 0x00 0x00 0x80 0x7F
  * @author  sdadz-luo
  */
/* JustFloat 帧长 = 通道数×4B + 帧尾 4B，供调用方预留缓冲 */
#define VOFA_FRAME_LEN(ch_count)  ((uint16_t)(ch_count) * 4U + 4U)

/* JustFloat 组帧：读 channels 的 count 路 float 写入 out（容量须 ≥
   VOFA_FRAME_LEN(count)），返回帧长；只做字节转换，不碰任何 IO */
uint16_t vofa_pack_frame(const float *channels, uint8_t count, uint8_t *out);

#endif /* VOFA_H */
