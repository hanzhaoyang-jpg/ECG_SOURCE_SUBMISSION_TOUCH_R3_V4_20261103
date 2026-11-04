# Vivado 构建复现检查

本目录有两项独立内容：[`vivado-repro-check/SKILL.md`](vivado-repro-check/SKILL.md) 是通用构建证据核查流程；`ecg_annotation/web/ecg_rpeak_label_trainer.html` 是 ECG R 峰/SQI 标注与回放网页，`ecg_annotation/test_web_ecg_logic.js` 是合成数据测试。

适用于把 Vivado 工程交给另一支队伍复现时，核查工程实际从哪里加载输入，以及构建进行到了哪一步。输入包括独立解压目录、构建脚本、Vivado 日志和本次输出。

按 [SKILL.md](vivado-repro-check/SKILL.md) 操作。该流程已用于 V4 工程，核对了四个自定义 IP 仓库路径，并在流程退出后确认了新比特流和硬件交接文件。只有部分日志时，可以报告已完成的阶段。日志没有覆盖的文件访问需要另行追踪。

## ECG 标注实例

网页本地打开，导入至少含 `timestamp_ms` 和 `raw_adc` 两列的 CSV；人工标注 R 峰与 SQI 区间，比较因果滤波、质量门控和检测参数，导出标签及候选 C 参数。运行 `node ecg_annotation/test_web_ecg_logic.js` 可检查部分滤波和 SQI 逻辑。该测试已在合成数据上通过，不代表真实记录或板端一致性已验证。

可复用的是“保留原始采样、人工标注、离线回放、误差分解、跨记录复核”流程；用于其他传感器或板卡时须重新适配 CSV 列名、标注类别、评价容差和算法。人工标签不完整、时间单位错误或只在同一记录调参与评估时，结果可能失真。网页不用于临床诊断，真实 ECG 与导出标签不要直接放入公开仓库。
