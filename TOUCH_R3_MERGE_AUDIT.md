# ECG 触摸 r3 修复合并审计（V3 → V4）

## 输入与范围
- 基线：用户重新上传的 `ECG_SOURCE_SUBMISSION_CLEAN_V3_20261103(1).zip`。
- 变更证据：`change_backup_since_handoff_20261103.zip`，按文件修改时间收集的 106 文件备份，而非 Git commit diff。
- 只将两份板端源码文件及 r3 版本/构建路径合并。V3 其余程序、IP、ROM、BSP 与上位机保持字节不变。

## 核心触摸修复
- `ecg_touch.c`: drive_low() 切换 GPIO 为输出后将输出数据锁存位清零；START 时检查 SDA/SCL 是否确实被拉低；初始化时先设置输出使能、输出方向及锁存，再释放 SCL/SDA、拉高 RESET。
- 备份说明表示通过 JTAG 看到了 `TAP TOP LEFT`，之后按钮可点击。这是用户提供的实机观察，**不是我在本环境中重新测得**。
- `ecg_standalone.h`：offline/workstation 版本标识 r2 → r3。

## 有选择的构建脚本合并
- `build_standalone.ps1`：只更新 r3 输出目录；继续支持 V3 的 `$env:XILINX_SDK_2018_3` 环境变量和 `-Sdk` 参数。
- `make_standalone_boot.ps1`：更新 r3 目录与 ELF 版本匹配字符串；**保留 V3 的 per-profile `configs/offline` / `configs/workstation` 配置复制**。备份脚本把所有 profile 的 ECGCFG.INI 都取自 standalone_tests，是构建配置回退风险，未引入。
- 不合并备份里大量 `.o/.exe/.elf/.BIN`、旧的 JTAG Tcl、FPGA.zip、tools.zip 与本机日志。

## 验证边界
- 源码变更均记录 SHA-256；测试结果见 `TOUCH_R3_TEST_REPORT.json`。
- 主机测试不等于真实 FPGA I²C、Vivado 2018.3 完整编译或 SD 冷启动。
- 建议后续使用匹配 r3 的 ELF/Bitstream/FSBL 做 SD 冷启动验收；不要只替换 ELF 或根据单次 JTAG 测试宣称冷启动通过。
