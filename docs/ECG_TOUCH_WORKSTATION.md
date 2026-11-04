# ATK-7084 触摸屏与 ECG Workstation 接入

目标硬件：正点原子领航者 Zynq7020，ATK-7084 / ATK-7" RGB TFTLCD-V1.3（800×480）。

## 安装与操作

1. 断开人体电极、断电，备份 SD 卡原来的 `BOOT.BIN` 与 `ECGCFG.INI`。先用台架信号验证，不用人体连接测试网口或触摸。
2. 单机使用选择 `build_output/touch_20261103/r3/offline/sd_card/`；与工作站联调选择同一层的 `workstation/sd_card/`。将所选目录里的两个文件放到 SD 卡根目录。r3 需要重新生成或使用经确认的对应 r3 启动镜像，不能沿用 r2 BOOT.BIN。r3 需要重新生成或使用经确认的对应 r3 启动镜像，不能沿用 r2 BOOT.BIN。不要将新 ELF 与旧 bitstream 混用。
3. 设置 SD 启动、上电。触摸区依次提示 `TAP TOP LEFT`、`TAP TOP RIGHT`、`TAP BOTTOM LEFT`。在对应屏幕角内约一指宽的位置，单指按住约 0.1 秒后松开。每次开机需重新确认方向；角点操作不执行记录或设置命令。
4. 波形下面显示 `RECORD/STOP`、`GAIN -`、`GAIN +`、`HP`、`LP`、`SAVE`。按下并在同一按钮内松手才生效。先停记录再调参数；`SAVE` 保存当前参数到 SD 卡。触摸不可用时，可用板载 PS_KEY0/1 操作。
5. 默认 780 倍、4 Hz 高通、40 Hz 低通，仍自动记录约 120 秒。需要只手动开录，在 `ECGCFG.INI` 中设置 `auto_record=0`。取卡前须看到本次记录已结束且 `SD:CLOSED`、`E:0`。

## 电脑实时监看

- 先接网线、确认链路灯，再启动联网版。电脑有线 IPv4 设为 `192.168.7.1/24`，板端为 `192.168.7.10/24`，UDP 目标端口 `5005`。隔离的直连链路不需要网关或 DNS。
- 在电脑运行用户提供的 `ECG-Workstation.exe`。如系统阻止接收，仅为可信专用网络放行 UDP 5005。工作站收包后应切换到实时模式，收包计数增长。
- 联网版沿用旧 BSP 的上电 PHY 初始化，有协商等待；不接网线或不需要监看时用离线版。拔插网线后的重连表现尚待真机验证。
- 工作站的“记录”保存的是电脑收到的副本，不控制板端 SD；工作站的 R 峰和 HR/RR 是前端估计，不是板端测值。v1 未提供实时 SQI、AFE 状态或远程设置，界面对应空项是预期行为。
- v1 波形值为固件滤波数值乘 1000 的有符号整数，并未标定为物理 mV。不要把工作站网格当成电压计量或临床结果；不以工作站收包计数证明端到端零丢样。

## TOUCH OFFLINE 排查

旧版的 `TOUCH OFFLINE` 表示没有成功读到 I2C 帧，不表示校准失败。画面能亮只证明 RGB 显示链路工作。先断电检查 TFT 排线两端是否插到底、锁扣是否扣牢，确认使用包含新触摸 bitstream 的完整 BOOT.BIN。

r3 沿用相同的 bitstream，并包含 r2 屏幕诊断及触摸 GPIO/I²C 修复，不需要重新综合：

| 提示 | 固件观测 |
| --- | --- |
| `TOUCH: SCL LOW` | 释放时钟后仍读到低电平；优先查时钟线、上拉、排线 |
| `TOUCH: SDA LOW` | 开始或结束传输时，数据线释放后仍为低电平 |
| `TOUCH: NO ACK AT 0X38` | 第一次写地址未应答；查屏模组触摸供电、复位、排线和实际芯片版本 |
| `TOUCH: REGISTER NACK` / `TOUCH: READ NACK` | 第一地址已应答，失败在寄存器索引或读地址阶段 |
| `TAP TOP LEFT` | 已经读到帧，进入方向校准 |

提示是故障发生阶段，不是对具体坏件的判定。暂不随机改 FPGA 引脚或在带电状态下重插排线。

## 实现与验证边界

- 不引入 Qt。RGB888 帧缓冲、VDMA 和波形局部扫屏保留；触摸条和状态区仅在状态变化时重画。
- EMIO GPIO 扩展至 6 位。原 CN121 RESET/LOD 仍用位 0/1。位 2~5 对应 SCL=V17、SDA=M19、RST=Y17、INT=V18。FT5206/FT5426 使用 0x38 地址；轮询不需要触摸中断。GPIO 模拟 I2C 的 SCL/SDA 只拉低或释放，异常事务有限返回。
- 按键和触摸复用同一记录和设置逻辑；不新增远程控制。原有 CNN、采样频率、滤波参数和 SD 记录格式未替换。
- Windows 工作站测试使用提供的 EXE、回环 UDP 和合成 v1 样本，核对了实际 SSE 输出。不等同于真机网络验证。首次实物测试依次检查三个方向点、六个操作键、保存后重启、关闭文件及联网模式。

## 构建入口

```powershell
& '<VIVADO_2018_3>/bin/vivado.bat' -mode batch -source tools/vivado/vivado_direct_build.tcl -tclargs build_output/touch_20261103
& '<SDK_2018_3>/bin/xsct.bat' tools/vivado/build_standalone_fsbl.tcl touch
& .\tools\build_standalone.ps1 -Profile offline
& .\tools\build_standalone.ps1 -Profile workstation
& .\tools\make_standalone_boot.ps1 -Profile offline
& .\tools\make_standalone_boot.ps1 -Profile workstation
```

打包还需要本轮的 `tests.log`、`lcd_tests.log`；分别运行 `tools/test_standalone.ps1` 与 `tools/test_lcd_display.ps1`，将输出记录至 `build_output/touch_20261103/r2/`。用 `tools/verify_standalone_package.py` 加两个版本目录分别核验镜像、版本、配置和 HDF/FSBL 配对。

接线依据：领航者 SDK 开发指南第 31 章，第 560–561 页；触摸帧定义参见 FocalTech FT5426/5526 应用说明。


## r3 触摸 I²C 拉低修复（2026-11-03）

- 源码入口：`d_cnn2/d_cnn2/d_cnn2.sdk/SD_state/src/ecg_touch.c`。GPIO 方向寄存器改为输出时，显式把对应输出数据锁存位置 0；新增 START 后 SDA/SCL 拉低读回诊断，并调整 RESET、SCL、SDA 初始化先后顺序。
- 已有 JTAG 实机反馈：可以出现 `TAP TOP LEFT` 并使用触摸按钮；仅证明当次 JTAG 加载配对运行，**未证明 r3 SD 冷启动**。
- `ecg_standalone.h` 的离线/工作站 firmware revision 均为 `r3`；构建脚本输出到 `build_output/touch_20261103/r3/{offline,workstation}`，保留现有 `configs/<profile>/ECGCFG.INI` 的按配置复制逻辑。
- r3 旧诊断 Tcl（probe/scan/run）是历史排障证据，本参赛源码包不包含；构建与运行不依赖。
- 备份中的 r3 BOOT.BIN 属于**历史生成参考**，不混入本次源码候选包；本包里的 r3 输出应在用户本机用 Vivado/SDK 2018.3 重新构建。
