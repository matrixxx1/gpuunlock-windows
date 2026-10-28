param([string]$Python = 'python')
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.Windows.Forms
Add-Type -AssemblyName System.Drawing

$helper = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'Set-BootDefault.ps1')).Path
$statusScript = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot 'Status-GpuUnlock.ps1')).Path
$defaultState = Join-Path $env:LOCALAPPDATA 'GPUUnlockWindows\boot-default.txt'
$startupPath = Join-Path ([Environment]::GetFolderPath('Startup')) 'GPU Unlock Control.lnk'
$self = $PSCommandPath
$application = New-Object System.Windows.Forms.ApplicationContext
$tray = New-Object System.Windows.Forms.NotifyIcon
$tray.Icon = [System.Drawing.SystemIcons]::Shield
$tray.Text = 'GPU Unlock Control'
$menu = New-Object System.Windows.Forms.ContextMenuStrip
$menu.AutoClose = $true
$statusItem = $menu.Items.Add('Current status: Checking...')
$statusItem.Enabled = $false
$refreshItem = $menu.Items.Add('Refresh current status')
$menu.Items.Add('-') | Out-Null
$unlockItem = $menu.Items.Add('Unlock during boot')
$lockedItem = $menu.Items.Add('Run Normal')
$menu.Items.Add('-') | Out-Null
$startItem = $menu.Items.Add('Start with Windows')
$stopItem = $menu.Items.Add("Don't start with Windows")
$menu.Items.Add('-') | Out-Null
$exitItem = $menu.Items.Add('Exit')
$tray.ContextMenuStrip = $menu
$statusProcess = $null
$statusTimer = New-Object System.Windows.Forms.Timer
$statusTimer.Interval = 500
$dismissTimer = New-Object System.Windows.Forms.Timer
$dismissTimer.Interval = 50

function Update-StartupMenu {
  $enabled = Test-Path -LiteralPath $startupPath
  $startItem.Checked = $enabled
  $stopItem.Checked = !$enabled
}
function Update-DefaultMenu {
  $mode = if (Test-Path -LiteralPath $defaultState) {
    (Get-Content -LiteralPath $defaultState -Raw).Trim()
  } else { 'Normal' }
  $unlockItem.Checked = $mode -eq 'Unlock'
  $lockedItem.Checked = $mode -ne 'Unlock'
}
function Install-StartupShortcut {
  $shell = New-Object -ComObject WScript.Shell
  $shortcut = $shell.CreateShortcut($startupPath)
  $shortcut.TargetPath = 'powershell.exe'
  $shortcut.Arguments = '-NoProfile -ExecutionPolicy Bypass -WindowStyle Hidden -File "' + $self + '" -Python "' + $Python + '"'
  $shortcut.WorkingDirectory = $PSScriptRoot
  $shortcut.Description = 'Run GPU Unlock Control in the system tray after Windows sign-in'
  $shortcut.Save()
}
function Set-BootMode([string]$mode) {
  Start-Process powershell.exe -ArgumentList @(
    '-NoProfile','-ExecutionPolicy','Bypass','-File',('"'+$helper+'"'),'-Mode',$mode)
}
function Show-TrayMenu {
  Update-StartupMenu
  Update-DefaultMenu
  if ($menu.Visible) { $menu.Close() }
  $menu.Show([System.Windows.Forms.Cursor]::Position)
}
function Start-StatusCheck {
  if ($script:statusProcess -and !$script:statusProcess.HasExited) { return }
  $statusItem.Text = 'Current status: Checking...'
  $refreshItem.Enabled = $false
  $info = New-Object System.Diagnostics.ProcessStartInfo
  $info.FileName = 'powershell.exe'
  $info.Arguments = '-NoProfile -ExecutionPolicy Bypass -File "' + $statusScript + '" -Python "' + $Python + '" -NoMessageBox'
  $info.UseShellExecute = $false
  $info.CreateNoWindow = $true
  $info.RedirectStandardOutput = $true
  $info.RedirectStandardError = $true
  $script:statusProcess = New-Object System.Diagnostics.Process
  $script:statusProcess.StartInfo = $info
  if (!$script:statusProcess.Start()) {
    $statusItem.Text = 'Current status: Unable to start check'
    $refreshItem.Enabled = $true
    return
  }
  $statusTimer.Start()
}

$unlockItem.Add_Click({ Set-BootMode 'Unlock' })
$lockedItem.Add_Click({ Set-BootMode 'Normal' })
$refreshItem.Add_Click({ Start-StatusCheck })
$startItem.Add_Click({ Install-StartupShortcut; Update-StartupMenu })
$stopItem.Add_Click({
  if (Test-Path -LiteralPath $startupPath) { Remove-Item -LiteralPath $startupPath -Force }
  Update-StartupMenu
})
$exitItem.Add_Click({
  $tray.Visible = $false
  $application.ExitThread()
})
$tray.Add_MouseClick({
  param($sender,$eventArgs)
  if ($eventArgs.Button -eq 'Left') { Show-TrayMenu }
})
$statusTimer.Add_Tick({
  if (!$script:statusProcess -or !$script:statusProcess.HasExited) { return }
  $statusTimer.Stop()
  $output = $script:statusProcess.StandardOutput.ReadToEnd()
  $errorText = $script:statusProcess.StandardError.ReadToEnd()
  $code = $script:statusProcess.ExitCode
  $script:statusProcess.Dispose()
  $script:statusProcess = $null
  $summary = @($output -split "`r?`n" | Where-Object { $_ -match '^(UNLOCKED|LOCKED|INDETERMINATE) - ' }) | Select-Object -Last 1
  if ($code -eq 0 -and $summary) { $statusItem.Text = 'Current status: ' + $summary }
  elseif ($code -eq 1 -and $summary) { $statusItem.Text = 'Current status: ' + $summary }
  elseif ($code -eq 2 -and $summary) { $statusItem.Text = 'Current status: ' + $summary }
  else { $statusItem.Text = 'Current status: Unable to verify' }
  $tray.Text = if ($code -eq 0) { 'GPU Unlock Control - UNLOCKED' }
               elseif ($code -eq 1) { 'GPU Unlock Control - LOCKED' }
               else { 'GPU Unlock Control - status unknown' }
  $refreshItem.Enabled = $true
})
$dismissTimer.Add_Tick({
  if (!$menu.Visible) { return }
  if ([System.Windows.Forms.Control]::MouseButtons -ne [System.Windows.Forms.MouseButtons]::None -and
      !$menu.Bounds.Contains([System.Windows.Forms.Cursor]::Position)) {
    $menu.Close()
  }
})
Update-StartupMenu
Update-DefaultMenu
$tray.Visible = $true
$dismissTimer.Start()
$tray.ShowBalloonTip(2500, 'GPU Unlock Control',
  'Left-click or right-click the tray icon for next-boot options.', 'Info')
Start-StatusCheck
try {
  [System.Windows.Forms.Application]::Run($application)
} finally {
  $tray.Visible = $false
  $tray.Dispose()
  $statusTimer.Stop()
  $statusTimer.Dispose()
  $dismissTimer.Stop()
  $dismissTimer.Dispose()
  if ($script:statusProcess -and !$script:statusProcess.HasExited) { $script:statusProcess.Kill() }
  if ($script:statusProcess) { $script:statusProcess.Dispose() }
  $menu.Dispose()
  $application.Dispose()
}
