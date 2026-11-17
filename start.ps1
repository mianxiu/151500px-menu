param([switch]$Headless, [int]$Port = 15151)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$menuPython = Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $menuPython)) {
    $menuPython = Join-Path $PSScriptRoot 'Scripts\python.exe'
}
if (-not (Test-Path -LiteralPath $menuPython)) {
    throw '请先执行 python -m venv .venv，并安装 requirements.txt 中的依赖。'
}
$menuArguments = @('main.py', '--port', "$Port")
if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'native\HotkeyHost.exe'))) {
    & (Join-Path $PSScriptRoot 'tools\build-hotkey-host.ps1')
}
if ($Headless) { $menuArguments += '--headless' }
if (!$Headless -and !(Test-Path -LiteralPath (Join-Path $PSScriptRoot 'native\control\ControlHost.exe'))) {
    & (Join-Path $PSScriptRoot 'tools\build-control-host.ps1')
}
if (-not (Test-Path -LiteralPath (Join-Path $PSScriptRoot 'native\MenuHost.exe'))) {
    & (Join-Path $PSScriptRoot 'tools\build-menu-host.ps1')
}
& $menuPython @menuArguments
exit $LASTEXITCODE
