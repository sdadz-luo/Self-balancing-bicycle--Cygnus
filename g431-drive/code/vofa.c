#include "headline.h"

/**
  * @file    vofa.c
  * @brief   VOFA JustFloat 组帧：float[] → 小端字节 + 帧尾，纯转换
  * @author  sdadz-luo
  */

/* JustFloat 帧尾：float +inf 的 IEEE-754 小端表示（0x7F800000）。
   上位机以此为帧界，两帧背靠背也可靠它自同步——丢整帧无副作用 */
static const uint8_t vofa_frame_tail[4] = {0x00U, 0x00U, 0x80U, 0x7FU};

uint16_t vofa_pack_frame(const float *channels, uint8_t count, uint8_t *out)
{
    uint16_t len = 0U;

    if ((channels == NULL) || (out == NULL) || (count == 0U))
    {
        return 0U;
    }

    /* Cortex-M 本身小端，float 按 IEEE-754 小端 memcpy 铺开即协议
       要求，无需字节序调整 */
    for (uint8_t i = 0U; i < count; i++)
    {
        memcpy(&out[len], &channels[i], sizeof(float));
        len += (uint16_t)sizeof(float);
    }

    memcpy(&out[len], vofa_frame_tail, sizeof(vofa_frame_tail));
    len += (uint16_t)sizeof(vofa_frame_tail);

    return len;
}
