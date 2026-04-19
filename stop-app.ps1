$ErrorActionPreference = "Stop"

$root = $PSScriptRoot
Set-Location $root
$pidFile = Join-Path $root ".app-pids.json"

$stoppedAny = $false

function Stop-IfRunning {
  param(
    [int]$PidToStop
  )

  try {
    $proc = Get-Process -Id $PidToStop -ErrorAction Stop
    Stop-Process -Id $proc.Id -Force
    Write-Host "Stopped process PID $($proc.Id) ($($proc.ProcessName))."
    return $true
  } catch {
    return $false
  }
}

if (Test-Path $pidFile) {
  try {
    $saved = Get-Content -Path $pidFile -Raw | ConvertFrom-Json
    if ($saved.backendShellPid) {
      if (Stop-IfRunning -PidToStop ([int]$saved.backendShellPid)) { $stoppedAny = $true }
    }
    if ($saved.frontendShellPid) {
      if (Stop-IfRunning -PidToStop ([int]$saved.frontendShellPid)) { $stoppedAny = $true }
    }
  } catch {
    Write-Warning "Could not read PID file. Falling back to port-based stop."
  }

  Remove-Item -Path $pidFile -ErrorAction SilentlyContinue
}

# Fallback: free known app ports in case PID file is missing/stale.
$ports = @(8080, 5500)
foreach ($port in $ports) {
  try {
    $listeners = Get-NetTCPConnection -LocalPort $port -State Listen -ErrorAction Stop
    foreach ($conn in $listeners) {
      if (Stop-IfRunning -PidToStop ([int]$conn.OwningProcess)) { $stoppedAny = $true }
    }
  } catch {
    # No listener on this port.
  }
}

if (-not $stoppedAny) {
  Write-Host "No running app processes were found."
}
