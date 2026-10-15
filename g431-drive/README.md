# g431-drive —— 电驱（STM32G431RBT6）

双电机 FOC 电驱板固件：动量轮走力矩环（指令来自 ESP32 平衡环）、摩擦驱动走
速度环，含 DRV8301 / MT6816 驱动与硬件保护。

- 硬件：STM32G431RBT6 + DRV8301 ×2 + MT6816 ×2，图纸见
  [hardware/drive-board](../hardware/drive-board/)
- 工具链：STM32CubeMX + Keil MDK5
- 文档：[电驱设计方案](../docs/天鹅座-STM32G431RBT6_双电机FOC设计方案.md)
- 构建方式与改动禁区见 [CLAUDE.md](CLAUDE.md)
