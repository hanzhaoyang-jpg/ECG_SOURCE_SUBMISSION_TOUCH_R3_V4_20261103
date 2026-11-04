# 基于 Zynq-7020 的长时 ECG 监测系统｜参赛源码候选包（触摸 r3 修复合并 V4）

**SUBMISSION CANDIDATE / 不代表公开授权已确认，也不代表从零构建通过。** 原始 ZIP 不修改。

## 工程构成
- Vivado 2018.3: `d_cnn2/d_cnn2/d_cnn2.xpr`，PL 逻辑、BD、约束、自定义 IP
- Xilinx SDK 2018.3: `d_cnn2/d_cnn2/d_cnn2.sdk/SD_state/src`，BSP、链接脚本
- 原有 HLS CNN: `d_cnn_3/d_cnn_3/src` 和集成 IP
- Go 工作站: `ECG-Workstation/src`，本地 Web，UDP v1
- 测试与构建入口：`tools/`；离线/联网配置：`configs/`

## 关键约束
CN121 + AD9280，360 Hz；保留既有滤波、R 峰、SQI、CNN、LCD、SD 格式和有线 UDP v1（5005，28 字节）。只有 workstation 模式联网。未替换为新的研究 CNN。

## Windows 构建（需 Vivado/SDK 2018.3 工具链）
在**工程根目录**的 PowerShell 中，按以下顺序运行。将工具路径替换为本机安装路径：

```powershell
$V = 'D:/tool/Vivado/2018.3/bin/vivado.bat'
$S = 'D:/tool/SDK/2018.3'
& $V -mode batch -source tools/vivado/vivado_direct_build.tcl -tclargs build_output/touch_20261103
& "$S/bin/xsct.bat" tools/vivado/build_standalone_fsbl.tcl touch
& .\tools\build_standalone.ps1 -Sdk $S -Profile offline
& .\tools\build_standalone.ps1 -Sdk $S -Profile workstation
& .\tools\test_standalone.ps1
& .\tools\test_lcd_display.ps1
& .\tools\make_standalone_boot.ps1 -Sdk $S -Profile offline
& .\tools\make_standalone_boot.ps1 -Sdk $S -Profile workstation
```

工作站在 `ECG-Workstation/src` 执行 `go build ./...`，可运行 `python tools/test_workstation_udp.py` 的兼容测试（需按脚本要求准备服务或测试环境）。

这些命令为**待验收复现步骤**，本包未在真实 Xilinx 2018.3 环境执行通过。项目脚本中部分测试/manifest 依赖输出日志，构建应先保存相应测试结果，再产出正式启动包。

`reference_build_artifacts` 是旧工程生成的 bit/hdf/hwdef 参考对照，不是本次从零生成。若只需重建软件，可执行 `tools/stage_reference_hardware.ps1` 暂存其副本（仍不等于全量构建验收）。

见 `SOURCE_AUDIT.md`、`THIRD_PARTY_NOTICES.md`、`FILE_MANIFEST_SHA256.json` 和 `docs/ECG_TOUCH_WORKSTATION.md`。

## 触摸 r3 修复
已将经 JTAG 验证的 GPIO/I²C 拉低源码修复合并进 `ecg_touch.c`，保留 V3 构建脚本的 SDK 可配置路径及 profile 区分；详见 `TOUCH_R3_MERGE_AUDIT.md`。r3 SD 冷启动仍待验证。

## 调试代码清理审计
请阅读 `DEBUG_REDUNDANCY_AUDIT.md`；本版仅移出可追溯的历史诊断文件并补齐三份漏包测试源。正常 UART 故障诊断、RTL/IP 和 SDK BSP 均保留。

# 第三方组件与版权声明（THIRD_PARTY_NOTICES）

本项目《基于ZYNQ的长时心电监测系统》在设计与实现过程中，使用了部分第三方开发工具、IP 核、硬件驱动及软件依赖。相关组件的著作权及其他知识产权归其原权利人所有。

本项目尊重并保留第三方组件原有的版权声明、署名及许可信息。第三方组件的集成使用不代表其原始实现属于本项目自主原创成果。本项目的其他许可声明不自动覆盖第三方组件。

## 1. AMD/Xilinx Vivado/SDK 2018.3 相关组件

本项目采用 AMD/Xilinx Zynq-7000 开发平台，并使用 Vivado 2018.3 和 Xilinx SDK 2018.3 完成硬件设计、IP 集成及嵌入式软件开发。

相关第三方组件包括：

- Xilinx SDK 自动生成的 BSP、驱动与处理器初始化代码；
- Zynq Processing System 相关配置与初始化文件；
- Vivado Block Design 和 Xilinx IP 生成文件；
- 与硬件平台相关的其他 Xilinx 开发资源。

上述文件中属于 AMD/Xilinx 提供的代码及工具生成资源，其版权和许可条件遵循对应权利人的规定。本项目保留原始版权声明与许可标识，不将其作为独立原创成果。

对于需要公开再分发的生成源码、预编译库和其他受许可约束的资源，将依据其适用许可条件决定是否纳入公开仓库；未纳入的构建依赖将在项目文档中说明获取或重新生成方式。

## 2. LCD、VDMA 与触摸驱动相关组件

本项目的显示与人机交互系统集成了 RGB LCD、AXI VDMA 以及触摸控制相关组件。

其中，LCD 显示链路使用了**正点原子（ALIENTEK）提供的 `rgb2lcd` IP 核**，用于实现 RGB 视频信号与 LCD 显示接口的连接。

该 IP 核及其原始代码的著作权归原权利人所有。本项目保留相关源文件和 `Copyright.txt` 中的原始版权声明，并在技术文档中注明来源。

其他 LCD、VDMA 和触摸驱动文件，将按照实际来源分别标识 AMD/Xilinx、正点原子或其他原始权利人。对于经过修改的第三方代码，保留原始署名，并记录本项目所作的实际修改。

第三方组件的使用及公开再分发以对应许可条件为准。尚未确认公开再分发权限的组件，不因本项目已注明出处而自动获得再分发授权。

## 3. 上位机 Go 语言依赖及界面资源

本项目电脑工作站采用 Go 语言实现本地服务，并通过 Web 前端进行心电波形和状态展示。

Go 语言相关依赖以项目 `go.mod` 和 `go.sum` 中的实际模块清单为准。各模块的著作权、许可证及再分发条件分别遵循其原始项目的许可要求。

项目将保留适用的第三方版权声明，并在公开发布前核对相关依赖的许可信息。

对于图标、字体、图片等界面资源，仅纳入已确认具有合法使用及再分发权限的文件；尚未核实授权的资源不纳入公开源码发布版本。

---

## 版权与许可说明

本项目中的自主设计内容与第三方组件分别进行权属管理。

项目对第三方资源的引用、集成及必要修改，均以尊重原作者权利、保留版权声明和遵守适用许可条件为原则。

本文件用于说明第三方组件的来源及权属情况，不构成原权利人的授权证明。对于尚未明确许可范围的组件，项目将在取得相应许可或确认合法再分发条件后决定其公开发布方式。
