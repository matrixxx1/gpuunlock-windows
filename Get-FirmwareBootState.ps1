param([Parameter(Mandatory=$true)][string]$Output)
$ErrorActionPreference = 'Stop'
$text = (& bcdedit.exe /enum firmware 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw "bcdedit failed: $text" }
$text | Set-Content -LiteralPath $Output -Encoding utf8
