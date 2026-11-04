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
