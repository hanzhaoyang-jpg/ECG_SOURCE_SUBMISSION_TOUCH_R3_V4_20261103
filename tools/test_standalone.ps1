param(
    [string]$Gcc = 'D:/tool/Vivado/2018.3/msys64/mingw64/bin/gcc.exe',
    [string]$ArmGcc = 'D:/tool/SDK/2018.3/gnu/aarch32/nt/gcc-arm-none-eabi/bin/arm-none-eabi-gcc.exe'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$src = Join-Path $root 'd_cnn2/d_cnn2/d_cnn2.sdk/SD_state/src'
$bsp = Join-Path $root 'd_cnn2/d_cnn2/d_cnn2.sdk/SD_state_bsp/ps7_cortexa9_0/include'
$test = Join-Path $PSScriptRoot 'standalone_tests'
$out = Join-Path $root 'build_output/standalone_20261028/tests'
New-Item -ItemType Directory -Path $out -Force | Out-Null
$oldPath = $env:PATH
function Check([string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "$program failed with $LASTEXITCODE" }
}
try {
    $env:PATH = (Split-Path $Gcc -Parent) + ';' + $oldPath
    Check $Gcc @('-std=c99','-O2','-Wall','-Wextra','-Werror','-DECG_SETTINGS_HOST_TEST',
        "-I$bsp","-I$src","-I$test","$test/test_standalone.c", "$src/ecg_standalone.c",
        "$src/ecg_settings_sd.c",'-o',"$out/settings.exe")
    Check "$out/settings.exe" @("$test/ECGCFG.INI")
    Check $Gcc @('-std=c99','-Wall','-Wextra','-Werror','-DECG_KEYS_HOST_TEST',
        "-I$bsp","-I$src","-I$test","$test/test_board_keys.c", "$src/ecg_board_keys.c",'-o',"$out/keys.exe")
    Check "$out/keys.exe" @()
    Check $Gcc @('-std=gnu99','-O2','-Wall','-Wextra','-Werror','-ffunction-sections',
        '-fdata-sections','-Wl,--gc-sections',"-I$bsp","-I$src",
        "$test/test_touch.c",'-o',"$out/touch.exe")
    Check "$out/touch.exe" @()
    Check $Gcc @('-std=gnu99','-O2','-flto','-Wall','-Wextra','-Werror','-ffunction-sections',
        '-fdata-sections','-Wl,--gc-sections',"-I$bsp","-I$src", "$test/test_main_controls.c",
        "$src/device_state.c","$src/ecg_standalone.c",'-o',"$out/main_controls.exe")
    Check "$out/main_controls.exe" @()
    Check $Gcc @('-std=gnu99','-O2','-flto','-Wall','-Wextra','-Werror',
        '-ffunction-sections','-fdata-sections','-Wl,--gc-sections',"-I$bsp","-I$src",
        "$test/test_recorder_close.c",'-o',"$out/recorder_close.exe")
    Check "$out/recorder_close.exe" @()
    Check $Gcc @('-std=c99','-O2','-Wall','-Wextra','-Werror','-DECG_CNN_HOST_TEST',"-I$bsp","-I$src","-I$PSScriptRoot",
        "$PSScriptRoot/test_cnn_async.c","$src/ecg_result.c",'-lm','-o',"$out/cnn_async.exe")
    Check "$out/cnn_async.exe" @()
    Check $ArmGcc @('-std=gnu99','-Wall','-Wextra','-Werror','-fsyntax-only',
        '-mcpu=cortex-a9','-mfpu=vfpv3','-mfloat-abi=hard',"-I$bsp","-I$src",
        "$src/main.c","$src/cn121_ps.c","$src/device_state.c","$src/ecg_recorder.c",
        "$src/ecg_standalone.c","$src/ecg_settings_sd.c","$src/ecg_board_keys.c","$src/lcd_wave.c","$src/ecg_touch.c")
    Write-Output 'Standalone host tests and ARM syntax checks passed.'
} finally { $env:PATH = $oldPath }
