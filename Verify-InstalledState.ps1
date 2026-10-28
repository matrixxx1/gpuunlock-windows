param([Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$marker = 'U:\AD103M-UNLOCK\BOOT-DEFAULT'
$firmware = (& bcdedit.exe /enum firmware 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'Unable to read firmware boot state' }
[ordered]@{
  persistent_default_exists = [bool](Test-Path -LiteralPath $marker)
  firmware = $firmware
} | ConvertTo-Json | Set-Content -LiteralPath $Output -Encoding utf8
