set root [file normalize [file join [file dirname [info script]] ..]]
set sdkdir [file join $root d_cnn2 d_cnn2 d_cnn2.sdk]
setws $sdkdir
catch {deleteprojects -name design_1_wrapper_hw_platform_eth} _
createhw -name design_1_wrapper_hw_platform_eth -hwspec [file join $sdkdir design_1_wrapper.hdf]
