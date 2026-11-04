# Vivado Batch Scripts

- `vivado_build.tcl`：调用工程的 `synth_1`、`impl_1` run，并生成报告。
- `vivado_direct_build.tcl`：直接综合、优化、布局、布线并生成 bitstream。
- `vivado_export_hdf.tcl`：由 bit 与 hwdef 导出 HDF。
- `vivado_hw_export_help.tcl`：查询 `write_sysdef` 帮助的临时辅助脚本。

脚本面向 Vivado 2018.3，并在顶部保存了当前电脑上的绝对路径。

