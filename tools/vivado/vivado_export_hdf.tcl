set root [file normalize [file join [file dirname [info script]] ../..]]
set output_dir "$root/build_output"
set project_file "$root/d_cnn2/d_cnn2/d_cnn2.xpr"
set hwdef_file "$output_dir/design_1_wrapper.hwdef"
set bit_file "$output_dir/design_1_wrapper.bit"
set hdf_file "$output_dir/design_1_wrapper.hdf"

open_project $project_file
write_sysdef -force \
    -hwdef $hwdef_file \
    -bitfile $bit_file \
    -file $hdf_file

puts "CODEX_HDF=$hdf_file"
close_project
exit
