param([string]$Sdk = $(if ($env:XILINX_SDK_2018_3) { $env:XILINX_SDK_2018_3 } else { 'D:/tool/SDK/2018.3' }),
      [ValidateSet('legacy','offline','workstation')][string]$Profile = 'legacy')
$ErrorActionPreference = 'Stop'
$root = Split-Path $PSScriptRoot -Parent
$src = Join-Path $root 'd_cnn2/d_cnn2/d_cnn2.sdk/SD_state/src'
$bsp = Join-Path $root 'd_cnn2/d_cnn2/d_cnn2.sdk/SD_state_bsp/ps7_cortexa9_0'
$out = Join-Path $root 'build_output/standalone_20261028'
if ($Profile -ne 'legacy') { $out = Join-Path $root "build_output/touch_20261103/r3/$Profile" }
$bin = Join-Path $Sdk 'gnu/aarch32/nt/gcc-arm-none-eabi/bin'
$gcc = Join-Path $bin 'arm-none-eabi-gcc.exe'
New-Item -ItemType Directory -Path "$out/objects" -Force | Out-Null
$oldPath = $env:PATH
function Check([string]$program, [string[]]$arguments) {
    & $program @arguments
    if ($LASTEXITCODE -ne 0) { throw "$program failed with $LASTEXITCODE" }
}
try {
    $env:PATH = "$bin;$(Join-Path $Sdk 'gnuwin/bin');$oldPath"
    $flags = @('-std=gnu99','-Wall','-Wextra','-O2','-g3','-mcpu=cortex-a9',
        '-mfpu=vfpv3','-mfloat-abi=hard',"-I$($bsp)/include","-I$src")
    if ($Profile -ne 'legacy') { $flags += @('-DECG_TOUCH_BUILD=1','-DLCD_FONT_SCALE=2') }
    if ($Profile -eq 'workstation') { $flags += '-DECG_WORKSTATION_BUILD=1' }
    $objects = @()
    foreach ($file in Get-ChildItem -LiteralPath $src -Filter '*.c' -Recurse | Sort-Object FullName) {
        $relative = $file.FullName.Substring($src.Length + 1).Replace('\','_').Replace('/','_')
        $object = Join-Path "$out/objects" ($relative + '.o')
        Check $gcc ($flags + @('-c',$file.FullName,'-o',$object))
        $objects += $object
    }
    if ($Profile -eq 'workstation') {
        # Link the patched Ethernet adapter before the bundled lwIP archive.
        $netif = Join-Path $bsp 'libsrc/lwip202_v1_2/src/contrib/ports/xilinx/netif'
        foreach ($name in 'xemacpsif.c', 'xemacpsif_hw.c') {
            $object = Join-Path "$out/objects" ($name + '.o')
            Check $gcc ($flags + @('-c', (Join-Path $netif $name), '-o', $object))
            $objects += $object
        }
    }
    $elf = Join-Path $out 'SD_state.elf'
    Check $gcc (@('-mcpu=cortex-a9','-mfpu=vfpv3','-mfloat-abi=hard',
        '-Wl,-build-id=none',"-specs=$PSScriptRoot/standalone_startup.spec", "-Wl,-T,$src/lscript.ld", "-Wl,-Map,$out/SD_state.map",
        "-L$bsp/lib",'-o',$elf) + $objects + @('-Wl,--start-group','-lxilffs','-llwip4','-lxil','-lm','-lc','-lgcc','-Wl,--end-group'))
    Check (Join-Path $bin 'arm-none-eabi-size.exe') @($elf)
    Check (Join-Path $bin 'arm-none-eabi-readelf.exe') @('-l',$elf)
    Write-Output "APPLICATION_BUILD_COMPLETE $elf"
} finally { $env:PATH = $oldPath }
