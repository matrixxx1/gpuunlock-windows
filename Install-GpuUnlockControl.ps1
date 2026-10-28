param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
$app = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'GpuUnlockControl.ps1')).Path
$desktop = [Environment]::GetFolderPath('Desktop')
$shortcutPath = Join-Path $desktop 'GPU Unlock Control.lnk'
$oldShortcut = Join-Path $desktop 'Shut down, unlock on next boot.lnk'
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = 'powershell.exe'
$shortcut.Arguments = '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + $app + '" -Python "' + $Python + '"'
$shortcut.WorkingDirectory = $PSScriptRoot
$shortcut.Description = 'Choose locked or unlocked mode for the next cold boot'
$shortcut.Save()
if (Test-Path -LiteralPath $oldShortcut) { Remove-Item -LiteralPath $oldShortcut -Force }
Write-Output "Installed desktop control panel: $shortcutPath"
