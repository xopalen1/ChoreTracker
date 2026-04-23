$ErrorActionPreference = "Stop"

$scriptRoot = $PSScriptRoot
$root = Split-Path -Parent $scriptRoot
$backendRoot = Join-Path $root "backend"
$src = Join-Path $backendRoot "src"
$outDir = Join-Path $backendRoot "bin"
$outFile = Join-Path $outDir "roommate_backend.exe"

if (!(Test-Path $outDir)) {
  New-Item -ItemType Directory -Path $outDir | Out-Null
}

$files = @(
  (Join-Path $src "main.c"),
  (Join-Path $src "net.c"),
  (Join-Path $src "http.c"),
  (Join-Path $src "server.c"),
  (Join-Path $src "router.c"),
  (Join-Path $src "db.c"),
  (Join-Path $src "json_utils.c"),
  (Join-Path $src "text_utils.c"),
  (Join-Path $src "string_builder.c"),
  (Join-Path $src "date_utils.c"),
  (Join-Path $src "reassignment.c"),
  (Join-Path $src "handlers_common.c"),
  (Join-Path $src "handlers_chores.c"),
  (Join-Path $src "handlers_messages.c"),
  (Join-Path $src "handlers_roommates.c")
)

$cl = Get-Command cl -ErrorAction SilentlyContinue
$gcc = Get-Command gcc -ErrorAction SilentlyContinue

if ($cl) {
  Push-Location $backendRoot
  & cl /std:c23 /W4 /I include /Fe:$outFile $files ws2_32.lib
  Pop-Location
  Write-Host "Built with MSVC: $outFile"
  exit 0
}

if ($gcc) {
  & gcc -std=c2x -Wall -Wextra -I "$backendRoot/include" @files -o "$outFile" -lws2_32
  Write-Host "Built with GCC: $outFile"
  exit 0
}

Write-Error "No supported C compiler found. Install MSVC Build Tools or MinGW GCC."
