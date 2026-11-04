# Offline generation only: no connect, reset, fpga, dow or flash operations.
set root [file normalize [file join [file dirname [info script]] ../..]]
set output "$root/build_output/standalone_20261028"
set hdf "$root/build_output/status_pins_20261008/design_1_wrapper.hdf"
if {[llength $argv] == 1 && [lindex $argv 0] eq "touch"} {
    set output "$root/build_output/touch_20261103"
    set hdf "$output/design_1_wrapper.hdf"
}
setws "$output/fsbl_workspace"
if {![file exists "$output/fsbl_workspace/ecg_hw/.project"]} {
    createhw -name ecg_hw -hwspec $hdf
}
if {![file exists "$output/fsbl_workspace/ecg_fsbl/.project"]} {
    createapp -name ecg_fsbl -app {Zynq FSBL} -hwproject ecg_hw -proc ps7_cortexa9_0 -os standalone
}
projects -build -type bsp -name ecg_fsbl_bsp
set library "$output/fsbl_workspace/ecg_fsbl_bsp/ps7_cortexa9_0/lib/libxil.a"
if {![file exists $library]} {error "FSBL BSP did not build: $library"}
projects -build -type app -name ecg_fsbl
set elf "$output/fsbl_workspace/ecg_fsbl/Debug/ecg_fsbl.elf"
if {![file exists $elf]} {error "FSBL compiler failed; ELF was not produced"}
puts "FSBL_BUILD_COMPLETE $output/fsbl_workspace/ecg_fsbl/Debug/ecg_fsbl.elf"
exit
