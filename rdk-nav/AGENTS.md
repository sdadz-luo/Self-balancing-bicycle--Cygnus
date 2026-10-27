# rdk-nav —— 决策端（RDK X5）工作约定

ROS 2 工作空间：感知接入、双 EKF 定位融合、Nav2 规划，只输出「目标速度 +
目标航向」建议值（20 ~ 50 Hz），经 CAN 交给 ESP32 仲裁。工程尚未建立。

## 代码管理：板上开发，Windows 管版本

**板上 `~/cygnus_ws/src/` 是开发区；本目录是归档副本 + 唯一的版本历史。**

- 日常读写代码用 SSH MCP 直接操作板子，**不在 Windows 侧编辑 `src/`**
  （否则下次拉取会被无提示覆盖）
- 收工前拉回（在 `D:\project\tools` 下执行）：

  ```powershell
  .\sync-board.ps1 -TargetHost RDKX5 -Pull -Local ..\Self-balancing-bicycle--Cygnus\rdk-nav -Remote cygnus_ws/src
  ```

- 拉回后在 Windows 提交 git——这是板上代码唯一的版本历史
- **只拉 `src/`**：colcon 的 `build/ install/ log/` 在工作空间根，不在 src 下
- ⚠️ `scp -r` 只增不减：板上删除文件后，Windows 侧要手动删对应文件

## 构建

板上 `colcon build --symlink-install`；非交互执行前先 `source ~/rdk-env.sh`。

## 改动禁区（改了会牵连别处）

- **输出永远是建议**——RDK 掉线不得影响控制回路（ESP32 侧 200 ms 超时降级）
- **地理原点一经确定不得更改**——卫星图配准、航点坐标、历史日志全依赖它
- **路径曲率上限随速度变化**（`κ ≤ g·tan20°/v²`），禁止「停-转-走」避障
  ——每次低速启停都在消耗飞轮动量（包线仅 ±3°）
- **导航域 IMU 用 GS130WI 板载的 ICM-42688-P**，不用经 CAN 转发的 HWT606
  （时间戳抖动是毫秒级）
- 坐标系统一最先做：WGS84 → ENU 局部平面 → `map` / `odom` / `base_link`

## 包切分（M5 阶段开始时建）

按「一个包只回答一个问题」切分：

| 问题 | 包 |
| --- | --- |
| 硬件怎么动 | `cygnus_bridge`（目标速度/航向 ↔ CAN） |
| 决策怎么做 | `cygnus_nav`（Nav2 参数、costmap 配置） |
| 怎么一键起 | `cygnus_bringup`（launch + 全局参数） |
| 消息长什么样 | `cygnus_msgs`（确需自定义消息时才建） |

感知类（GNSS / 双目 / 雷达）优先用现成驱动包，不自己写。

## 环境

板上 ROS 2 Humble + TROS（`/opt/ros/humble` + `/opt/tros/humble`）；连接方式与
环境细节见知识库设备档案 `设备/RDKX5.md`，此处不重复。
