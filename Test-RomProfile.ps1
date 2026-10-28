param(
    [Parameter(Mandatory = $true)][string]$Rom,
    [string]$Clang = 'clang'
)

$ErrorActionPreference = 'Stop'
$Rom = [IO.Path]::GetFullPath($Rom)
if (-not (Test-Path -LiteralPath $Rom -PathType Leaf)) {
    throw "ROM fixture not found: $Rom"
}

$output = Join-Path $PSScriptRoot 'build\test-ad103m-rom-profile.exe'
New-Item -ItemType Directory -Force (Split-Path $output) | Out-Null
Push-Location -LiteralPath $PSScriptRoot
try {
    & $Clang -std=c11 -O2 -Wall -Wextra -Werror -Wno-deprecated-declarations `
        -I . -I vendor -I vendor\edk2\Include -I vendor\edk2\Include\X64 `
        tests\test_ad103m_rom_profile.c ad103m_rom_profile.c sha256.c vendor\vbios_610.c `
        -o $output
    if ($LASTEXITCODE -ne 0) { throw 'AD103M ROM profile test compile failed.' }
    & $output $Rom
    if ($LASTEXITCODE -ne 0) { throw 'AD103M ROM profile test failed.' }
} finally {
    Pop-Location
}
