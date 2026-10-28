param(
  [string]$Python = 'python',
  [switch]$NoMessageBox,
  [switch]$KeepMeasurement
)
$ErrorActionPreference = 'Stop'
$measurement = Join-Path $env:TEMP ('gpu-unlock-status-' + [guid]::NewGuid().ToString('N') + '.json')
$title = 'GPU Unlock Status'
try {
  & $Python (Join-Path $PSScriptRoot 'benchmark.py') --out $measurement --label windows-status --samples 5 --batch 5 --warmup 0.5
  if ($LASTEXITCODE -ne 0) { throw "benchmark.py exited with code $LASTEXITCODE" }
  $data = Get-Content -LiteralPath $measurement -Raw | ConvertFrom-Json
  $fp16 = [double](($data.results | Where-Object mode -eq 'fp16_fp32').median_tflops)
  $bf16 = [double](($data.results | Where-Object mode -eq 'bf16_fp32').median_tflops)
  if ($fp16 -ge 120 -and $bf16 -ge 120) {
    $state = 'UNLOCKED'; $icon = 'Information'
    $message = "GPU is UNLOCKED for this boot.`n`nFP16: $($fp16.ToString('F1')) TFLOP/s`nBF16: $($bf16.ToString('F1')) TFLOP/s"
  } elseif ($fp16 -le 100 -and $bf16 -le 100) {
    $state = 'LOCKED'; $icon = 'Warning'
    $message = "GPU is LOCKED for this boot.`n`nFP16: $($fp16.ToString('F1')) TFLOP/s`nBF16: $($bf16.ToString('F1')) TFLOP/s`n`nChoose Unlock during boot, fully power down, wait 15 seconds, then power on."
  } else {
    $state = 'INDETERMINATE'; $icon = 'Warning'
    $message = "GPU status is INDETERMINATE.`n`nFP16: $($fp16.ToString('F1')) TFLOP/s`nBF16: $($bf16.ToString('F1')) TFLOP/s`n`nPower or thermal limits may have affected the measurement."
  }
  Write-Output "$state - FP16 $($fp16.ToString('F1')) TFLOP/s; BF16 $($bf16.ToString('F1')) TFLOP/s"
  if (!$NoMessageBox) {
    Add-Type -AssemblyName PresentationFramework
    [void][System.Windows.MessageBox]::Show($message, $title, 'OK', $icon)
  }
  if ($state -eq 'UNLOCKED') { exit 0 }
  if ($state -eq 'LOCKED') { exit 1 }
  exit 2
} catch {
  $message = "Unable to verify GPU unlock status.`n`n$($_.Exception.Message)"
  Write-Error $message
  if (!$NoMessageBox) {
    Add-Type -AssemblyName PresentationFramework
    [void][System.Windows.MessageBox]::Show($message, $title, 'OK', 'Error')
  }
  exit 3
} finally {
  if (!$KeepMeasurement -and (Test-Path -LiteralPath $measurement)) {
    Remove-Item -LiteralPath $measurement -Force
  }
}
