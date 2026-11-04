# ECG 参赛源码｜调试冗余审计与保守清理（2026-11-03）

## 结论
以用户上传的 `ECG_SOURCE_SUBMISSION_CANDIDATE_20261103_v2(1).zip` 为唯一源码基线。**保守清理，不改变运行时功能接口**。保留所有板端实际执行的 UART 诊断、CN121 配置读回、SD 写入故障日志、SQI/采样计数及 FIFO/CNN 故障恢复；它们并非无用调试代码。

### 本轮变更（可复核）
- **补齐 3 个真正缺失的构建测试源码**：`tools/test_cnn_async.c`、`tools/test_lcd_sweep.c`、`tools/test_lcd_flush.c`；三者来自用户提供的原始 `tools.zip`，原 V2 中的 PowerShell 脚本仍调用它们。
- **移出 12 个非当前构建入口文件**到独立历史归档：一份旧 HLS C++ 副本以及 11 个旧式 JTAG/状态/触摸诊断辅助文件。归档保留原始字节和路径，方便后续追溯。当前构建脚本不直接引用这些文件，不能推断项目从未使用过它们。
- 从 `cn121_ps.c` 清除两条已被 `//` 注释的打印和一段 `/* ... */` 注释打印，不改任何有效语句。
- 保留 `tools/vivado/README.md` 与其他正常文档，不按文件名或关键词误删 FPGA、BSP 或 IP 生成资源。

### 不建议删除的内容
| 文件/模块 | 表面调试内容 | 审计结论 |
|---|---|---|
| `SD_state/src/main.c` | 开机打印、每秒 ADC 与 SQI 状态、UART 控制菜单 | 仍在运行链路，便于现场故障定位；禁止未经对照移除 |
| `SD_state/src/cn121_ps.c` | SPI 写入错误、配置读回、参数调整输出、GPIO 调试查询 | 读回和报错属于配置正确性依据；本轮只清理被注释代码 |
| `SD_state/src/ecg_recorder.c` | FATFS f_write/f_sync/f_close 错误与状态 | 必须保留防止将损坏记录误判为完整文件 |
| `SD_state/src/ecg_cnn_infer.c` | DMA reset/timeout 等错误打印 | 关系到 CNN 与采集实时性诊断，不删除 |
| `SD_state_bsp/**` | 大量 `DEBUG`、`#if 0`、打印、自检 | Xilinx SDK 第三方 BSP，修改将破坏重建与许可证边界 |
| `d_cnn2/d_cnn2.srcs/**` | 生成的 sim/synth HDL、IP 配置与初始化文件 | 部分是重建输入，未执行 Vivado 不证明可安全批量剔除 |
| `tools/standalone_tests/**` | 明确的测试模拟桩 | 测试资产非生产冗余，应保留 |

### 关键遗留风险
1. **无法在当前环境运行 Vivado/SDK 2018.3 原生综合、FSBL/ELF 构建和上板测试**。即使主机测试通过，也不能认定硬件等价。
2. `CN121` 的 `cn121_gpio_*_debug()` 对外查询函数仍存在，无法在未做全量符号调用链验证前保证可移除；本轮保留。
3. 公开再分发授权尚未确认：Xilinx BSP、HLS 权重、生成 IP、LCD/触摸驱动；保持待审而非假定许可。
4. 历史归档不属于公开提交源码，另行保管。若比赛要求提供全部调试历史，可单独追加归档，不应回灌主工程。

### 分类
`RETAIN`：当前 Vivado/SDK/HLS/Go 源码、全部 IP/ROM/BSP 输入、诊断错误路径及测试源。
`RESTORE`：V2 错漏的 3 个测试 C 文件。
`ARCHIVE`：12 个无当前构建引用的旧诊断/备份文件。
`CLEAN`：仅已注释的 CN121 打印语句。
`UNVERIFIED`：真正的板上/2018.3 工具链复现、IP 权属。
