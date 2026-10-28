param(
  [Parameter(Mandatory=$true)][ValidatePattern('^\{[0-9a-fA-F-]{36}\}$')][string]$Selector,
  [Parameter(Mandatory=$true)][ValidatePattern('^[A-Z]$')][string]$DriveLetter,
  [Parameter(Mandatory=$true)][string]$Receipt
)
$ErrorActionPreference = 'Stop'
$before = (& bcdedit.exe /enum firmware 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw "Unable to read firmware BCD: $before" }
$entry = (& bcdedit.exe /enum $selector 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0 -or $entry -notmatch ('device\s+partition=' + [regex]::Escape($DriveLetter + ':')) -or
    $entry -notmatch 'path\s+\\EFI\\BOOT\\BOOTX64\.EFI') {
  throw "Refusing to modify boot order: the reviewed selector entry no longer matches $DriveLetter`:\EFI\BOOT\BOOTX64.EFI"
}
& bcdedit.exe /set '{fwbootmgr}' displayorder $selector /addfirst | Out-Null
if ($LASTEXITCODE -ne 0) { throw "Unable to place selector first; bcdedit exit $LASTEXITCODE" }
$after = (& bcdedit.exe /enum firmware 2>&1 | Out-String)
if ($LASTEXITCODE -ne 0) { throw 'Unable to verify firmware BCD after update' }
$lines = $after -split "`r?`n"
$displayIndex = [Array]::FindIndex($lines, [Predicate[string]]{ param($line) $line -match '^displayorder\s+' })
if ($displayIndex -lt 0 -or $lines[$displayIndex] -notmatch [regex]::Escape($selector)) {
  throw 'Selector is not first in firmware displayorder after update'
}
if ($after -notmatch '(?m)^identifier\s+\{bootmgr\}\s*$') {
  throw 'Windows Boot Manager is missing after update'
}
[ordered]@{
  utc = [DateTime]::UtcNow.ToString('o')
  selector = $selector
  selector_device = "partition=$DriveLetter`:"
  selector_path = '\EFI\BOOT\BOOTX64.EFI'
  operation = 'persistent displayorder addfirst'
  windows_boot_manager_preserved = $true
  before = $before
  after = $after
} | ConvertTo-Json -Depth 3 | Set-Content -LiteralPath $Receipt -Encoding utf8
