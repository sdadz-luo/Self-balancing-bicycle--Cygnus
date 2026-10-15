# esp32s3-control —— 下位机（ESP32-S3）

控制域主控固件：平衡环、运动控制、状态机、指令仲裁、近场避障（超声波 ×3）。
IMU（HWT606）与超声波直挂本地，经 CAN 与电驱板、上位机通信。

- 硬件：ESP32-S3 + 外接 CAN 收发器，图纸见
  [hardware/control-board](../hardware/control-board/)
- 工具链：ESP-IDF
- 文档：[参数与配置·四、ESP32-S3 下位机](../docs/天鹅座-参数与配置.md)
- 构建方式与改动禁区见 [CLAUDE.md](CLAUDE.md)
