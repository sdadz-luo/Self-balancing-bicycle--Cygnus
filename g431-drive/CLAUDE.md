# g431-drive —— 电驱（STM32G431RBT6）工作约定

双电机 FOC 固件：动量轮走力矩环（指令来自 ESP32 平衡环）、摩擦驱动走速度环。
工程尚未建立；建立后沿用 CubeMX 标准布局（`Core/` `Drivers/` `MDK-ARM/` +
`*.ioc`），**不手工整理这些目录**——CubeMX 重新生成会打乱。

## 构建

Keil MDK5 打开 `MDK-ARM/*.uvprojx`；改 `.ioc` 后用 CubeMX 重新生成代码。

## 改动禁区（改了会牵连别处）

- **功率级按相电流 100 A 设计**（不是 60 A）——由 θ_rec = 3° 推出的硬指标
- **nFAULT 走 TIM1/TIM8 BKIN 硬件刹车**：故障到封锁 PWM 约 100 ns，不经过
  代码。CubeMX 的 Break 页五项必配（极性对齐 nFAULT、加输入滤波、
  AOE = Disable、BKLOCK = Enable、OSSI/OSSR），**勿用 fDTS 大分频档**
  （可达 µs 级，等于废掉刹车）
- **分流电阻开尔文连接**（0.25 mΩ）、DRV8301 增益 40 V/V；小电流段精度差是
  预期行为，**不得据此判定电流环故障**
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

- [电驱设计方案](../docs/天鹅座-STM32G431RBT6_双电机FOC设计方案.md)——引脚、
  时钟、画板与固件注意事项
- [技术方案·硬指标索引](../docs/天鹅座-技术方案.md)
