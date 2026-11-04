param(
    [string]$Gcc = 'D:/tool/Vivado/2018.3/msys64/mingw64/bin/gcc.exe',
    [string]$ArmGcc = 'D:/tool/SDK/2018.3/gnu/aarch32/nt/gcc-arm-none-eabi/bin/arm-none-eabi-gcc.exe'
)
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$src = Join-Path $root 'd_cnn2/d_cnn2/d_cnn2.sdk/SD_state/src'
$bsp = Join-Path $root 'd_cnn2/d_cnn2/d_cnn2.sdk/SD_state_bsp/ps7_cortexa9_0/include'
$oldPath = $env:PATH
$out = Join-Path $env:TEMP ('ecg-lcd-test-' + [guid]::NewGuid().ToString('N'))
New-Item -ItemType Directory -Path $out | Out-Null
function Run-Checked([string]$Program, [string[]]$Arguments) {
    & $Program @Arguments
    if ($LASTEXITCODE -ne 0) { throw "$Program failed: $LASTEXITCODE" }
}
try {
    $env:PATH = (Split-Path $Gcc -Parent) + ';' + $env:PATH
    foreach ($width in @(800,480)) {
        $height = if ($width -eq 800) { 480 } else { 272 }
        foreach ($raw in @(0,1)) {
            $exe = Join-Path $out "sweep-$width-$raw.exe"
            Run-Checked $Gcc @('-std=c99','-Wall','-Wextra','-Werror',
                "-DLCD_WIDTH=$width","-DLCD_HEIGHT=$height","-DLCD_SHOW_RAW_WAVE=$raw",
                "-I$bsp",(Join-Path $PSScriptRoot 'test_lcd_sweep.c'),
                "$src/ecg_ring.c","$src/ecg_result.c",'-o',$exe)
            Run-Checked $exe @()
        }
    }
    $exe = Join-Path $out 'flush.exe'
    Run-Checked $Gcc @('-std=c99','-O2','-flto','-ffunction-sections',
        '-fdata-sections','-Wl,--gc-sections',"-I$bsp",
        (Join-Path $PSScriptRoot 'test_lcd_flush.c'),"$src/lcd_port.c",'-o',$exe)
    Run-Checked $exe @()
    Run-Checked $ArmGcc @('-std=gnu99','-Wall','-Wextra','-Werror',
        '-fsyntax-only','-mcpu=cortex-a9','-mfpu=vfpv3','-mfloat-abi=hard',
        "-I$bsp","-I$src","$src/main.c","$src/lcd_wave.c","$src/lcd_port.c")
    Write-Output 'All LCD host tests and ARM syntax checks passed.'
} finally {
    $env:PATH = $oldPath
}
