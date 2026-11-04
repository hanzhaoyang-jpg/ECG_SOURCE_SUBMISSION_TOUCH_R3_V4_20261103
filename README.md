# Zynq-7020 长时 ECG 监测系统

系统通过 CN121 和 AD9280 以 360 Hz 采集 ECG。板端完成滤波、R 峰检测、信号质量评估和 CNN 推理，通过 LCD 显示并将记录写入 SD 卡。联网模式还可以通过有线网络向 Go 工作站发送数据，使用 UDP v1（端口 5005，每包 28 字节）。

## 推理 IP 

本提交版使用 `d_cnn_3` 推理 IP，Vivado BD 中的活动实例为 `d_cnn_3_0`，PS 通过 AXI DMA 向其发送 864 字节心搏输入。自有 `ecg_int8_pl` 模型仍在单独验证，**未纳入本提交版**。

## 工程目录

| 目录 | 内容 |
| --- | --- |
| `d_cnn2/d_cnn2/` | Vivado 工程、RTL、约束、自定义 IP 和 SDK 工程 |
| `d_cnn_3/d_cnn_3/src/` | CNN 的 HLS 源码 |
| `ECG-Workstation/src/` | Go 工作站和 Web 界面 |
| `tools/` | 构建与测试脚本 |
| `configs/` | 离线和联网模式的 SD 卡配置 |

## 构建 SD 卡镜像

在 Windows 上安装 Vivado 和 Xilinx SDK 2018.3。在仓库根目录打开 PowerShell，将下面的工具路径改为实际安装位置。

```powershell
$V = 'D:/tool/Vivado/2018.3/bin/vivado.bat'
$S = 'D:/tool/SDK/2018.3'
$Profile = 'offline'  # 联网模式用 workstation

& $V -mode batch -source tools/vivado/vivado_direct_build.tcl -tclargs build_output/touch_20261103
& "$S/bin/xsct.bat" tools/vivado/build_standalone_fsbl.tcl touch
& .\tools\build_standalone.ps1 -Sdk $S -Profile $Profile
& .\tools\make_standalone_boot.ps1 -Sdk $S -Profile $Profile
```

将 `build_output/touch_20261103/r3/<模式>/sd_card/` 下的 `BOOT.BIN` 和 `ECGCFG.INI` 一起复制到 SD 卡根目录。打包脚本先写入这两个文件，再生成验收清单。生成清单还需要主机测试日志、工作站程序和 UDP 测试日志，所需路径见 `tools/make_standalone_boot.ps1`。

`build_output/` 是上述脚本在本地运行时创建的输出目录，不是提交包必须预先包含的输入。这里的命令展示构建顺序，尚不能视为从干净副本完成了端到端复现：`make_standalone_boot.ps1` 在生成镜像后还会要求 `tests.log`、`lcd_tests.log`、`workstation_udp_test.log` 和 `ECG-Workstation.exe` 才能写出完整验收清单；这些文件未随包提供，也不会由上面四条命令全部生成。`verify_standalone_package.py` 对 offline/workstation 还引用旧版 `build_output/status_pins_20261008/design_1_wrapper.hdf` 作地址映射对照，该历史 HDF 不在提交包内。`stage_reference_hardware.ps1` 的默认输入 `reference_build_artifacts/` 也未随包提供，不能把它当作新构建步骤。重新构建应用时还需核对 SDK BSP 与本次 HDF 的硬件地址映射。现有 `build/reports/` 保存的是此前构建报告，不是执行上述命令时自动生成的报告。

## 运行工作站

在 `ECG-Workstation/src` 下运行 `go build ./...`。联网版使用有线直连：电脑地址设为 `192.168.7.1/24`，板端默认 `192.168.7.10/24`，UDP 端口 `5005`；离线版不需要网线。触摸首次启用时按屏幕提示依次点左上、右上、左下，触摸失效可使用板载按键。板端 SD 记录与电脑收到的数据副本是两条独立路径。

## 验证记录

V4 源码已用 Vivado 2018.3 完成综合、布局布线和比特流生成。布线后满足工程指定的时序约束。离线和联网两种启动镜像均已生成。开发板完成了一次上电测试，长时间运行仍待验证。

第三方组件来源和待核对授权见 [THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md)。设计说明及 AI 协作记录见 `report/`，验证报告见 `build/` 和 `board/`。

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
