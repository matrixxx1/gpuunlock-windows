param(
  [Parameter(Mandatory=$true)][ValidateSet('Unlock','Normal')][string]$Mode,
  [switch]$NoMessageBox
)
$ErrorActionPreference = 'Stop'
$identity = [Security.Principal.WindowsIdentity]::GetCurrent()
$principal = New-Object Security.Principal.WindowsPrincipal($identity)
if (!$principal.IsInRole([Security.Principal.WindowsBuiltInRole]::Administrator)) {
  $args = '-NoProfile -ExecutionPolicy Bypass -File "' + $PSCommandPath + '" -Mode ' + $Mode
  if ($NoMessageBox) { $args += ' -NoMessageBox' }
  Start-Process -FilePath powershell.exe -Verb RunAs -ArgumentList $args
  exit
}
$partition = Get-Partition -DiskNumber 0 -PartitionNumber 4
if ($partition.DriveLetter -ne 'U' -or $partition.IsSystem -or
    $partition.GptType -ne '{c12a7328-f81f-11d2-ba4b-00a0c93ec93b}') {
  throw 'Dedicated experimental ESP identity does not match Disk 0 Partition 4 at U:'
}
$marker = 'U:\AD103M-UNLOCK\BOOT-DEFAULT'
$text = if ($Mode -eq 'Unlock') {
  'GPUUNLOCK-WINDOWS-DEFAULT-UNLOCK-V1'
} else {
  'GPUUNLOCK-WINDOWS-DEFAULT-NORMAL-V1'
}
$token = [Text.Encoding]::ASCII.GetBytes($text)
[IO.File]::WriteAllBytes($marker, $token)
$written = [IO.File]::ReadAllBytes($marker)
if ($written.Length -ne $token.Length -or
    [Convert]::ToBase64String($written) -ne [Convert]::ToBase64String($token)) {
  throw 'Persistent boot-default verification failed'
}
$legacy = 'U:\AD103M-UNLOCK\UNLOCK-NEXT-BOOT'
if (Test-Path -LiteralPath $legacy) { Remove-Item -LiteralPath $legacy -Force }
$stateDirectory = Join-Path $env:LOCALAPPDATA 'GPUUnlockWindows'
New-Item -ItemType Directory -Path $stateDirectory -Force | Out-Null
Set-Content -LiteralPath (Join-Path $stateDirectory 'boot-default.txt') -Value $Mode -Encoding ascii -NoNewline
Add-Type -AssemblyName PresentationFramework
$description = if ($Mode -eq 'Unlock') { 'Unlock during boot' } else { 'Run Normal' }
if (!$NoMessageBox) {
  [void][System.Windows.MessageBox]::Show(
    "$description is now the saved 20-second timeout default.`n`nFully power down the computer, wait at least 15 seconds, then power it back on for the lock status to change.`n`nYou can still press U or L during the boot countdown to override this setting for that boot.",
    'GPU Unlock Control', 'OK', 'Information')
}
