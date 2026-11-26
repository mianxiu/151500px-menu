$ErrorActionPreference = 'Stop'
$menuRoot = Split-Path -Parent $PSScriptRoot
$menuCompiler = (Get-Command g++.exe -ErrorAction Stop).Source
& $menuCompiler -std=c++17 -O2 -Wall -Wextra -static -mwindows "$menuRoot\native\HotkeyHost.cpp" -o "$menuRoot\native\HotkeyHost.exe" -luser32 -lgdi32
if ($LASTEXITCODE -ne 0) { throw 'Native hotkey host build failed.' }
