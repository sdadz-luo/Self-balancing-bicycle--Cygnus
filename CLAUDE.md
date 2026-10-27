# CLAUDE.md —— 项目工作约定

「天鹅座」自平衡自行车四端统一仓库的工作约定。项目介绍与目录导航见
[README.md](README.md)；本文件只写**不可推导、且每次都要遵守**的约定。

## 权威文档

- **[docs/天鹅座-技术方案.md](docs/天鹅座-技术方案.md) 是技术决策的唯一事实源**
  ——技术方案变更必须先改它再改代码，不得只改代码
- 具体数值、寄存器、公式推导 → [参数与配置](docs/天鹅座-参数与配置.md)

## 跨端约定

- **[shared/can/](shared/can/) 是 CAN 三端（STM32 / ESP32 / RDK）的接口契约**：
  改 CAN 帧定义等于改这三端代码；新增帧必须落入既有 ID 分配段（安全 / 控制 /
  电驱状态 / 规划 / 整车状态 / 保护）。遥控器是独立第四端，经 NRF24L01 接
  ESP32，其帧契约见参数 4.4
- 四端各自独立构建，**布局沿用各工具链的标准结构**（CubeMX / ESP-IDF /
  colcon），不手工整理生成目录——会被代码生成器打乱

## 构建入口

| 端 | 目录 | 构建方式 |
| --- | --- | --- |
| 电驱 | [g431-drive/](g431-drive/) | Keil MDK5 打开 `MDK-ARM/*.uvprojx` |
| 下位机 | [esp32s3-control/](esp32s3-control/) | `idf.py build` |
| 遥控器 | [f103-remote/](f103-remote/) | Keil MDK5（同电驱） |
| 决策端 | [rdk-nav/](rdk-nav/) | 板上 `colcon build --symlink-install` |

各端的改动禁区（改了会牵连别处的硬约束）见该端目录下的 `CLAUDE.md`。
