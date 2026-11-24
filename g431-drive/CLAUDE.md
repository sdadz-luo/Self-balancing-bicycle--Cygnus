# g431-drive —— 电驱（STM32G431RBT6）工作约定

双电机 FOC 固件：动量轮走力矩环（指令来自 ESP32 平衡环）、摩擦驱动走速度环
（**速度环闭环在 STM32 本地**，ESP32 只发目标车速）。
工程基线已入库（f634ac2），布局沿用 CubeMX 标准结构（`Core/` `Drivers/`
`MDK-ARM/` + `*.ioc`），**不手工整理这些目录**——CubeMX 重新生成会打乱。

## 构建

Keil MDK5 打开 `MDK-ARM/*.uvprojx`；改 `.ioc` 后用 CubeMX 重新生成代码。

## code/ 自写代码骨架

固件自写代码在 [code/](code/)，独立于 CubeMX 生成目录，Keil 工程 `code`
组已注册全部文件；**新增文件要同步挂进 `*.uvprojx`**，否则不参与编译。

- **include 统一走 [code/headline.h](code/headline.h)**（聚合头）：各模块
  `.h` 汇入它，各 `.c` 只 include 这一个头
- **应用代码写 [code/app.c](code/app.c)**：初始化序列 + 主循环，由
  `main()` 调用；`main.c` 只留 CubeMX 生成的 `MX_*_Init` 与对 app 的调用
- **protect.c/h 承载保护类逻辑**：freeze 位配置（`protect_init`，设计方案
  3.2 第 5 条）+ CSS 受控停机（3.2 第 2 条）
- 其余模块一文件一职责、成对 `.c/.h`：adc / spi / drv8301 / mt6816 / uart（驱动层），
  pid / svpwm / foc / motor（算法层），vofa（VOFA 波形组帧，协议层），
  can / isr（中断集中）/ brake_chopper（母线泄放）
- **printf 只在主循环上下文用**：fputc 是攒行缓冲，行尾必须带 `\n` 否则
  积压不发；ISR 里发数据一律走 `uart_send_bytes` / `uart_send_wave`。
  工程已勾 **Use MicroLIB**（uvprojx `useUlib=1`）——printf 走 fputc 全靠
  它；重配工程时丢了这个勾，标准库半主机依赖会撞链接错误

## 改动禁区（改了会牵连别处）

- **功率级按相电流 100 A 设计**（不是 60 A）——由 θ_rec = 3° 推出的硬指标
- **nFAULT 走 TIM1/TIM8 BKIN 硬件刹车**：故障到封锁 PWM 约 100 ns，不经过
  代码（**这是硬件一半**；完整链 = DRV 自关本桥 + BKIN 锁另一片 + ISR 里补清
  故障侧 MOE / 拉 EN_GATE / 置标志，且 PA15/PD2 与 TIM1/TIM8 是**交叉映射**
  ——见设计方案 3.2 第 4 条）。CubeMX 的 Break 页五项必配（极性对齐 nFAULT、加输入滤波、
  AOE = Disable、BKLOCK = Enable、OSSI/OSSR），**勿用 fDTS 大分频档**
  （可达 µs 级，等于废掉刹车）
- **分流电阻开尔文连接**（**1 mΩ**，与实物原理图一致）、DRV8301 增益 **10 V/V**
  （等效跨阻与 0.25 mΩ × 40 V/V 相同，满量程 ±135 A / +165 A）；小电流段精度差
  是预期行为，**不得据此判定电流环故障**
- VDS 过流阈值与 OCP 模式必须配置（锁存关断档，保证过流拉 nFAULT 进 BKIN 链）
- **IWDG 的启动延后是硬要求**：G4 HAL 的 `HAL_IWDG_Init` 内部即启动（无独立
  Start API），若 `main()` 自动调用，初始化即放狗、上电序列中途被咬。
  IWDG 留在 CubeMX 配置（P32 / W20 / R40），但 **Advanced Settings 里
  IWDG 的 Generate Function Call 必须保持不勾**——`MX_IWDG_Init()` 只在
  `app_init` 序列末显式调用（调用时机即启动时机）。勾回去等于废掉这个
  设计；每次重新生成后 `grep MX_IWDG_Init main.c` 确认初始化序列里没有它
- 母线与功率器件耐压 ≥ 50 V + TVS 钳位（飞轮卸载会回灌能量）
- 上电验证走**分阶段**流程（只焊 MCU → 加 DRV8301 不焊 MOSFET → 限流电源带
  小电机 → 台架逐级升流 → 才接真飞轮），不得跳级——把硬件问题与控制问题
  彻底分离

## 参考

- **硬件事实源**：[hardware/drive-board](../hardware/README.md) 原理图——V1.0
  **已整板焊装**（驱动板 3 页 + 主控板，H1/H2 互连）；**文档与图不一致时以图为准**
  （约定与已核对清单见 hardware/README）
- [电驱设计方案](../docs/天鹅座-STM32G431RBT6_双电机FOC设计方案.md)——引脚、
  时钟、画板与固件注意事项
- [技术方案·硬指标索引](../docs/天鹅座-技术方案.md)
