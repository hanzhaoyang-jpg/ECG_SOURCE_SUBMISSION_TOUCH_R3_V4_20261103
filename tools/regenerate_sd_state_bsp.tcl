set root [file normalize [file join [file dirname [info script]] ..]]
set sdkdir [file join $root d_cnn2 d_cnn2 d_cnn2.sdk]
setws $sdkdir
regenbsp -bsp SD_state_bsp

set ff_path [file join $sdkdir SD_state_bsp/ps7_cortexa9_0/libsrc xilffs_v4_0 src ff.c]
if {[file exists $ff_path]} {
    set fp [open $ff_path r]
    set txt [read $fp]
    close $fp
    set txt [string map [list \
        "\tint len,\t\t/* Size of string buffer (items) */" \
        "\ts32 len,\t\t/* Size of string buffer (items) */"] $txt]
    set fp [open $ff_path w]
    puts -nonewline $fp $txt
    close $fp
}

set hls_mk [file join $sdkdir SD_state_bsp/ps7_cortexa9_0/libsrc d_cnn_3_v1_0 src Makefile]
if {[file exists $hls_mk]} {
    set fp [open $hls_mk r]
    set txt [read $fp]
    close $fp
    regsub {\n\tmake clean\n} $txt "\n" txt
    set fp [open $hls_mk w]
    puts -nonewline $fp $txt
    close $fp
}

projects -build -type bsp -name SD_state_bsp
