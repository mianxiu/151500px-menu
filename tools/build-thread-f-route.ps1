param([string]$OutputDirectory = '', [switch]$Low)
$ErrorActionPreference = 'Stop'
$menuRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $menuRoot '.runtime/thread-f-route/build' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$menuCompiler = (Get-Command g++.exe -ErrorAction Stop).Source
$menuTest = Join-Path $OutputDirectory 'test-thread-f-route.exe'
& $menuCompiler -std=c++17 -O2 -Wall -Wextra -static -municode (Join-Path $PSScriptRoot 'test-thread-f-route.cpp') -o $menuTest -luser32 -limm32 -lole32 -loleaut32 -luuid
if ($LASTEXITCODE -ne 0) { throw 'Thread F isolated prototype build failed.' }
if ($Low) { & $menuTest --low } else { & $menuTest }
if ($LASTEXITCODE -ne 0) { throw 'Thread F isolated prototype failed.' }
