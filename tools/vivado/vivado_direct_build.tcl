# Resolve relative to this script, not the shell's current working directory.
# Layout: <root>/tools/vivado/vivado_direct_build.tcl
#         <root>/d_cnn2/d_cnn2/d_cnn2.xpr
set script_dir [file dirname [file normalize [info script]]]
set root_dir [file normalize [file join $script_dir ../..]]
set project_file [file join $root_dir d_cnn2 d_cnn2 d_cnn2.xpr]
if {![file isfile $project_file]} {
    error "Vivado project missing: $project_file (keep d_cnn2 and tools together under one root)"
}
if {![info exists output_dir]} {
    set output_dir [file join $root_dir build_output]
    if {[llength $argv] >= 1} {set output_dir [file normalize [lindex $argv 0]]}
}
set output_dir [file normalize $output_dir]
puts "ECG_PROJECT=$project_file"
puts "ECG_OUTPUT=$output_dir"
set part_name "xc7z020clg400-2"

file mkdir $output_dir
open_project $project_file
set_property top design_1_wrapper [current_fileset]
update_compile_order -fileset sources_1

set bd_file [get_files -quiet */design_1.bd]
if {[llength $bd_file] > 0} {
    open_bd_design $bd_file
    set_property CONFIG.PCW_GPIO_EMIO_GPIO_WIDTH 6 [get_bd_cells processing_system7_0]
    validate_bd_design -force
    save_bd_design
    reset_target all $bd_file
    generate_target all $bd_file
    close_bd_design [current_bd_design]
}

puts "CODEX_STAGE=synthesis"
set_msg_config -id {Synth 8-689} -new_severity ERROR
synth_design -top design_1_wrapper -part $part_name
write_checkpoint -force "$output_dir/post_synth.dcp"
report_utilization -file "$output_dir/post_synth_utilization.rpt"
report_timing_summary -file "$output_dir/post_synth_timing_summary.rpt"

puts "CODEX_STAGE=optimization"
opt_design

puts "CODEX_STAGE=placement"
place_design
phys_opt_design
write_checkpoint -force "$output_dir/post_place.dcp"

puts "CODEX_STAGE=routing"
route_design
write_checkpoint -force "$output_dir/post_route.dcp"
report_utilization -file "$output_dir/post_route_utilization.rpt"
report_timing_summary -file "$output_dir/post_route_timing_summary.rpt"
report_drc -file "$output_dir/post_route_drc.rpt"
report_io -file "$output_dir/post_route_io.rpt"

puts "CODEX_STAGE=bitstream"
set bit_file "$output_dir/design_1_wrapper.bit"
write_bitstream -force $bit_file

set hwdef_file "$output_dir/design_1_wrapper.hwdef"
write_hwdef -force -file $hwdef_file
write_sysdef -force -hwdef $hwdef_file -bitfile $bit_file -file "$output_dir/design_1_wrapper.hdf"

puts "CODEX_BITSTREAM=$bit_file"
puts "CODEX_HWDEF=$hwdef_file"
close_project
exit
