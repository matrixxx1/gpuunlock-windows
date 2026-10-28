param(
  [Parameter(Mandatory=$true)][string]$Python
)
$ErrorActionPreference = 'Stop'
$pythonPath = (Resolve-Path -LiteralPath $Python).Path
$statusScript = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'Status-GpuUnlock.ps1')).Path
$startup = [Environment]::GetFolderPath('Startup')
$shortcutPath = Join-Path $startup 'GPU Unlock Status.lnk'
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut($shortcutPath)
$shortcut.TargetPath = 'powershell.exe'
$shortcut.Arguments = '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + $statusScript + '" -Python "' + $pythonPath + '"'
$shortcut.WorkingDirectory = $PSScriptRoot
$shortcut.Description = 'Verify and display the volatile GPU unlock state after Windows sign-in'
$shortcut.Save()
Write-Output "Installed logon notifier: $shortcutPath"
