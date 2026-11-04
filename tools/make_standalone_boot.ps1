param([string]$Sdk = $(if ($env:XILINX_SDK_2018_3) { $env:XILINX_SDK_2018_3 } else { 'D:/tool/SDK/2018.3' }),
      [ValidateSet('legacy','offline','workstation')][string]$Profile = 'legacy')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$out = Join-Path $root 'build_output/standalone_20261028'
$fsbl = Join-Path $out 'fsbl_workspace/ecg_fsbl/Debug/ecg_fsbl.elf'
$bit = Join-Path $root 'build_output/status_pins_20261008/design_1_wrapper.bit'
$hdf = Join-Path $root 'build_output/status_pins_20261008/design_1_wrapper.hdf'
$testOut = $out
$firmware = 'standalone_nav_780_4hz_20261028'
if ($Profile -ne 'legacy') {
    $hardware = Join-Path $root 'build_output/touch_20261103'
    $testOut = Join-Path $hardware 'r3'
    $out = Join-Path $testOut $Profile
    $fsbl = Join-Path $hardware 'fsbl_workspace/ecg_fsbl/Debug/ecg_fsbl.elf'
    $bit = Join-Path $hardware 'design_1_wrapper.bit'
    $hdf = Join-Path $hardware 'design_1_wrapper.hdf'
    $firmware = "ecg_touch_${Profile}_20261103_r3"
}
$card = Join-Path $out 'sd_card'
$elf = Join-Path $out 'SD_state.elf'
foreach ($file in @($fsbl,$bit,$hdf,$elf)) {
    if (!(Test-Path -LiteralPath $file -PathType Leaf)) { throw "Missing build input: $file" }
}
$elfText = [Text.Encoding]::ASCII.GetString([IO.File]::ReadAllBytes($elf))
if (!$elfText.Contains($firmware)) { throw "ELF is not the requested $Profile profile" }
New-Item -ItemType Directory -Path $card -Force | Out-Null
$bif = Join-Path $out 'boot.bif'
$body = "the_ROM_image:`r`n{`r`n [bootloader] `"$($fsbl.Replace('\','/'))`"`r`n `"$($bit.Replace('\','/'))`"`r`n `"$($elf.Replace('\','/'))`"`r`n}`r`n"
[IO.File]::WriteAllText($bif, $body, [Text.Encoding]::ASCII)
& (Join-Path $Sdk 'bin/bootgen.bat') -arch zynq -image $bif -w on -o "$card/BOOT.BIN"
if ($LASTEXITCODE -ne 0) { throw "bootgen failed: $LASTEXITCODE" }
if ($Profile -eq 'legacy') { $cfg = Join-Path $PSScriptRoot 'standalone_tests/ECGCFG.INI' } else { $cfg = Join-Path $root "configs/$Profile/ECGCFG.INI" }
Copy-Item -LiteralPath $cfg -Destination "$card/ECGCFG.INI" -Force
$src = Join-Path $root 'd_cnn2/d_cnn2/d_cnn2.sdk/SD_state/src'
$hashes = [ordered]@{}
$inputs = @($fsbl,$bit,$hdf,$elf,$bif,"$card/BOOT.BIN","$card/ECGCFG.INI")
$inputs += @(Get-ChildItem -LiteralPath $src -Recurse -File | Where-Object { $_.Extension -in @('.c','.h','.ld') } | ForEach-Object FullName)
$inputs += @((Join-Path $PSScriptRoot 'build_standalone.ps1'),$PSCommandPath,(Join-Path $PSScriptRoot 'standalone_startup.spec'))
$inputs += @(Get-ChildItem -LiteralPath "$PSScriptRoot/standalone_tests" -File | ForEach-Object FullName)
$bsp = Join-Path $root 'd_cnn2/d_cnn2/d_cnn2.sdk/SD_state_bsp/ps7_cortexa9_0'
$inputs += @("$bsp/lib/libxil.a","$bsp/lib/libxilffs.a","$bsp/lib/liblwip4.a","$bsp/include/xparameters.h",
    "$PSScriptRoot/vivado/build_standalone_fsbl.tcl","$PSScriptRoot/verify_standalone_package.py",
    "$PSScriptRoot/test_standalone.ps1","$PSScriptRoot/test_lcd_display.ps1", "$testOut/tests.log", "$testOut/lcd_tests.log")
if ($Profile -ne 'legacy') {
    $rtl = Join-Path $root 'd_cnn2/d_cnn2/d_cnn2.srcs'
    $inputs += @("$rtl/sources_1/bd/design_1/design_1.bd",
        "$rtl/sources_1/new/design_1_wrapper.v","$rtl/constrs_2/new/ecg_board.xdc",
        "$PSScriptRoot/vivado/vivado_direct_build.tcl","$hardware/post_route_timing_summary.rpt",
        "$hardware/post_route_io.rpt","$hardware/post_route_drc.rpt",
        "$hardware/workstation_udp_test.log","$PSScriptRoot/test_workstation_udp.py",
        "$hardware/ECG-Workstation/ECG-Workstation.exe")
}
foreach ($file in $inputs) { $hashes[$file] = (Get-FileHash -LiteralPath $file -Algorithm SHA256).Hash.ToLowerInvariant() }
$manifest = [ordered]@{
    firmware = $firmware
    profile = $Profile
    hardware_hdf = $hdf
    fsbl_elf = $fsbl
    state = 'built_not_tested_on_physical_board'
    application_optimization = '-O2'
    cnn = 'existing_HLS_863_samples_plus_1_RR_not_the_new_segment_CNN'
    hardware_changed = ($Profile -ne 'legacy')
    board = 'ALIENTEK Navigator Zynq7020, user identified; not physically tested'
    touch = $(if ($Profile -eq 'legacy') {'disabled'} else {'ATK-7084_FT5206_FT5426'})
    default = @{gain=780; hp_x100=400; lp_hz=40; auto_record=1; start_delay_s=3; record_seconds=120; key_profile=1}
    card_root_files = @('BOOT.BIN','ECGCFG.INI')
    hashes = $hashes
}
[IO.File]::WriteAllText((Join-Path $out 'build_manifest.json'),($manifest | ConvertTo-Json -Depth 6),[Text.UTF8Encoding]::new($false))
Write-Output "SD_BOOT_PACKAGE_BUILT_NOT_BOARD_TESTED $card"
