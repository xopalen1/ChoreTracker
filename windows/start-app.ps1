$ErrorActionPreference = "Stop"

$scriptRoot = $PSScriptRoot
$root = Split-Path -Parent $scriptRoot
Set-Location $root
$pidFile = Join-Path $root ".app-pids.json"

$backendExe = Join-Path $root "backend\bin\roommate_backend.exe"
$backendBuildScript = Join-Path $scriptRoot "build-backend.ps1"

if (!(Test-Path $backendExe)) {
  Write-Host "Backend binary not found. Building backend..."
  & $backendBuildScript
}

$pythonCmd = $null
if (Get-Command py -ErrorAction SilentlyContinue) {
  $pythonCmd = "py"
} elseif (Get-Command python -ErrorAction SilentlyContinue) {
  $pythonCmd = "python"
} else {
  throw "Python not found. Install Python or add it to PATH."
}

$backendCommand = "Set-Location '$root'; .\backend\bin\roommate_backend.exe 8080"
$frontendCommand = "Set-Location '$root'; $pythonCmd -m http.server 5500 --bind 0.0.0.0"

$backendProcess = Start-Process powershell -ArgumentList "-NoExit", "-Command", $backendCommand -PassThru
$frontendProcess = Start-Process powershell -ArgumentList "-NoExit", "-Command", $frontendCommand -PassThru

$pids = [PSCustomObject]@{
  backendShellPid = $backendProcess.Id
  frontendShellPid = $frontendProcess.Id
}
$pids | ConvertTo-Json | Set-Content -Path $pidFile -Encoding UTF8

function Test-ValidLanIPv4 {
  param(
    [string]$Ip
  )

  if ([string]::IsNullOrWhiteSpace($Ip)) { return $false }
  if ($Ip -notmatch '^(?:\d{1,3}\.){3}\d{1,3}$') { return $false }
  if ($Ip -like '127.*' -or $Ip -like '169.254.*') { return $false }
  return $true
}

function Test-PrivateLanIPv4 {
  param(
    [string]$Ip
  )

  if (-not (Test-ValidLanIPv4 -Ip $Ip)) { return $false }
  if ($Ip -like '10.*') { return $true }
  if ($Ip -like '192.168.*') { return $true }
  if ($Ip -match '^172\.(1[6-9]|2\d|3[0-1])\.') { return $true }
  return $false
}

$primaryLanIp = $null
$allLanIps = @()
try {
  # Prefer the interface currently used for default route (actual internet/LAN path).
  $route = Get-NetRoute -DestinationPrefix "0.0.0.0/0" -ErrorAction SilentlyContinue |
    Sort-Object RouteMetric, InterfaceMetric |
    Select-Object -First 1

  if ($route) {
    $primaryLanIp = Get-NetIPAddress -AddressFamily IPv4 -InterfaceIndex $route.InterfaceIndex -ErrorAction SilentlyContinue |
      ForEach-Object { [string]$_.IPAddress } |
      Where-Object { Test-ValidLanIPv4 -Ip $_ } |
      Select-Object -First 1
  }

  $allLanIps = @(Get-NetIPConfiguration |
    Where-Object {
      $_.IPv4Address -and
      $_.InterfaceAlias -match "Wi-Fi|Wireless|WLAN|Ethernet" -and
      $_.InterfaceAlias -notmatch "vEthernet|VPN|Nord|Hyper-V|Virtual|Loopback|Bluetooth"
    } |
    ForEach-Object {
      foreach ($addr in $_.IPv4Address) {
        [string]$addr.IPAddress
      }
    } |
    Where-Object { Test-ValidLanIPv4 -Ip $_ } |
    Select-Object -Unique)

  $privateIps = @($allLanIps | Where-Object { Test-PrivateLanIPv4 -Ip $_ })
  if ($privateIps.Count -gt 0) {
    $allLanIps = $privateIps
  }

  if (-not (Test-ValidLanIPv4 -Ip $primaryLanIp) -and $allLanIps.Count -gt 0) {
    $primaryLanIp = $allLanIps[0]
  }
} catch {
  $primaryLanIp = $null
  $allLanIps = @()
}

Write-Host "Started backend on port 8080 and frontend on port 5500."
Write-Host "Open locally: http://localhost:5500"
if (Test-ValidLanIPv4 -Ip $primaryLanIp) {
  Write-Host "Open on same Wi-Fi: http://$($primaryLanIp):5500"
}

if ($allLanIps.Count -gt 1) {
  Write-Host "Other LAN addresses on this PC:"
  foreach ($ip in $allLanIps) {
    if ((Test-ValidLanIPv4 -Ip $ip) -and $ip -ne $primaryLanIp) {
      Write-Host "  http://$($ip):5500"
    }
  }
}
