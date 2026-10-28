param(
  [Parameter(Mandatory=$true)][string]$Image,
  [Parameter(Mandatory=$true)][string]$Receipt
)
$ErrorActionPreference = 'Stop'
$imagePath = (Resolve-Path -LiteralPath $Image).Path
$expectedImage = Join-Path $PSScriptRoot 'build\AD103M-EXPERIMENTAL.EFI'
if ($imagePath -ne $expectedImage) { throw 'Refusing an unexpected EFI image path' }
$partition = Get-Partition -DiskNumber 0 -PartitionNumber 4
if ($partition.DriveLetter -ne 'U' -or $partition.IsSystem -or
    $partition.GptType -ne '{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}') {
  throw 'Dedicated experimental ESP identity does not match Disk 0 Partition 4 at U:'
}
$targets = @('U:\EFI\BOOT\BOOTX64.EFI', 'U:\EFI\GPUUnlock\AD103M-EXPERIMENTAL.EFI')
foreach ($target in $targets) {
  if (!(Test-Path -LiteralPath $target -PathType Leaf)) { throw "Missing expected target: $target" }
}
$stamp = Get-Date -Format 'yyyyMMdd-HHmmss'
$backup = "U:\EFI\GPUUnlock\backups\$stamp"
New-Item -ItemType Directory -Path $backup -Force | Out-Null
Copy-Item -LiteralPath $targets[0] -Destination (Join-Path $backup 'BOOTX64.EFI')
Copy-Item -LiteralPath $targets[1] -Destination (Join-Path $backup 'AD103M-EXPERIMENTAL.EFI')
$sourceHash = (Get-FileHash -LiteralPath $imagePath -Algorithm SHA256).Hash
foreach ($target in $targets) {
  Copy-Item -LiteralPath $imagePath -Destination $target -Force
  if ((Get-FileHash -LiteralPath $target -Algorithm SHA256).Hash -ne $sourceHash) {
    throw "Staged hash mismatch: $target"
  }
}
[ordered]@{
  utc = [DateTime]::UtcNow.ToString('o')
  source = $imagePath
  sha256 = $sourceHash.ToLowerInvariant()
  targets = $targets
  backup = $backup
  bcd_changed = $false
} | ConvertTo-Json | Set-Content -LiteralPath $Receipt -Encoding utf8
