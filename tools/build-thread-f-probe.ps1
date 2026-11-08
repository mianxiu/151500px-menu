param([string]$OutputDirectory = '', [switch]$Low)
$ErrorActionPreference = 'Stop'
$menuRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $menuRoot '.runtime/thread-f-probe/build' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$menuCompiler = (Get-Command g++.exe -ErrorAction Stop).Source
$menuTest = Join-Path $OutputDirectory 'test-thread-f-probe.exe'
& $menuCompiler -std=c++17 -O2 -Wall -Wextra -static (Join-Path $PSScriptRoot 'test-thread-f-probe.cpp') -o $menuTest -luser32
if ($LASTEXITCODE -ne 0) { throw 'F observer fixture build failed.' }
if ($Low) { & $menuTest --low } else { & $menuTest }
if ($LASTEXITCODE -ne 0) { throw 'F observer fixture failed.' }
& $menuCompiler -std=c++17 -O2 -Wall -Wextra -shared -static (Join-Path $PSScriptRoot 'ThreadFProbe.cpp') -o (Join-Path $OutputDirectory 'ThreadFProbe.dll') -luser32
if ($LASTEXITCODE -ne 0) { throw 'F observer DLL build failed.' }
