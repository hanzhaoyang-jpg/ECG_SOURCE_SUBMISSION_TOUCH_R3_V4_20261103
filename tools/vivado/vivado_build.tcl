set root [file normalize [file join [file dirname [info script]] ../..]]
set project_file "$root/d_cnn2/d_cnn2/d_cnn2.xpr"
set output_dir "$root/build_output"

file mkdir $output_dir
open_project $project_file

set_property top design_1_wrapper [current_fileset]
update_compile_order -fileset sources_1

set bd_file [get_files -quiet */design_1.bd]
if {[llength $bd_file] > 0} {
    open_bd_design $bd_file
    validate_bd_design
    generate_target all $bd_file
    close_bd_design [current_bd_design]
}

reset_run synth_1
launch_runs synth_1 -jobs 1
wait_on_run synth_1

set synth_status [get_property STATUS [get_runs synth_1]]
puts "CODEX_SYNTH_STATUS=$synth_status"
if {![string match "*Complete*" $synth_status]} {
    error "Synthesis failed: $synth_status"
}

open_run synth_1
report_utilization -file "$output_dir/post_synth_utilization.rpt"
report_timing_summary -file "$output_dir/post_synth_timing_summary.rpt"

launch_runs impl_1 -to_step write_bitstream -jobs 1
wait_on_run impl_1

set impl_status [get_property STATUS [get_runs impl_1]]
puts "CODEX_IMPL_STATUS=$impl_status"
if {![string match "*Complete*" $impl_status]} {
    error "Implementation/bitstream failed: $impl_status"
}

open_run impl_1
report_utilization -file "$output_dir/post_route_utilization.rpt"
report_timing_summary -file "$output_dir/post_route_timing_summary.rpt"
report_drc -file "$output_dir/post_route_drc.rpt"

set bitfiles [get_property BITSTREAM.FILE [get_runs impl_1]]
puts "CODEX_BITSTREAM=$bitfiles"

set hwdef_file "$output_dir/design_1_wrapper.hwdef"
write_hwdef -force -file $hwdef_file
puts "CODEX_HWDEF=$hwdef_file"

close_project
exit
