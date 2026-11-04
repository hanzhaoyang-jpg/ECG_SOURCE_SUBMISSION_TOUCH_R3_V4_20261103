param([string]$Source = (Join-Path (Split-Path $PSScriptRoot -Parent) 'reference_build_artifacts'))
$ErrorActionPreference = 'Stop'
$Root = Split-Path $PSScriptRoot -Parent
$Dest = Join-Path $Root 'build_output/touch_20261103'
New-Item -ItemType Directory -Force -Path $Dest | Out-Null
foreach ($N in @('design_1_wrapper.bit','design_1_wrapper.hdf','design_1_wrapper.hwdef')) {
  $From = Join-Path $Source $N
  if (!(Test-Path -LiteralPath $From -PathType Leaf)) { throw "Missing $From" }
  Copy-Item -LiteralPath $From -Destination (Join-Path $Dest $N) -Force
}
Write-Output 'STAGED_REFERENCE_HARDWARE (prior historical artifacts; NOT a fresh Vivado build)'
