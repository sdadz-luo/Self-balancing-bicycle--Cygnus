# rdk-nav —— 上位机（RDK X5）

ROS 2 工作空间：GNSS 双天线 / 双目 / 激光雷达接入，双 EKF 定位融合，
Nav2 路径规划，只输出「目标速度 + 目标航向」建议值（20 ~ 50 Hz）。

- 硬件：RDK X5 8G（成品）
- 工具链：ROS 2 Humble + TROS
- 布局：ROS 2 包放 `src/`；`build/` `install/` `log/` 为构建产物，不入库
- 文档：[参数与配置·五、RDK X5 上位机](../docs/天鹅座-参数与配置.md)
- **代码在板上开发**，本目录是归档副本——工作流与改动禁区见
  [CLAUDE.md](CLAUDE.md)
