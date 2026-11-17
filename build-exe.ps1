param([switch]$SkipFrontend, [string]$OutputDirectory = 'dist')
$ErrorActionPreference = 'Stop'
$menuPython = Join-Path $PSScriptRoot '.venv\Scripts\python.exe'
if (-not (Test-Path -LiteralPath $menuPython)) {
    $menuPython = Join-Path $PSScriptRoot 'Scripts\python.exe'
}
if (-not (Test-Path -LiteralPath $menuPython)) {
    throw 'Create .venv and install requirements-build.txt before building.'
}
Push-Location $PSScriptRoot
try {
    & (Join-Path $PSScriptRoot 'tools\build-hotkey-host.ps1')
    & (Join-Path $PSScriptRoot 'tools\build-menu-host.ps1')
    & (Join-Path $PSScriptRoot 'tools\build-input-context.ps1')
    & (Join-Path $PSScriptRoot 'tools\build-input-service.ps1')
    & (Join-Path $PSScriptRoot 'tools\build-control-host.ps1')
    if (-not $SkipFrontend) {
        Push-Location (Join-Path $PSScriptRoot 'frontend')
        try {
            & npm ci
            if ($LASTEXITCODE -ne 0) { throw 'npm ci failed.' }
            & npm run build
            if ($LASTEXITCODE -ne 0) { throw 'Frontend build failed.' }
        } finally { Pop-Location }
    }
    if (-not (Test-Path -LiteralPath 'frontend/dist/index.html')) {
        throw 'Missing frontend build.'
    }
    & $menuPython -m PyInstaller --noconfirm --distpath $OutputDirectory 151500px-menu.spec
    if ($LASTEXITCODE -ne 0) { throw 'EXE build failed.' }
    $menuExe = Join-Path $OutputDirectory '151500px-menu.exe'
    $menuDigest = (Get-FileHash -LiteralPath $menuExe -Algorithm SHA256).Hash.ToLowerInvariant()
    "$menuDigest  151500px-menu.exe" | Set-Content -LiteralPath (Join-Path $OutputDirectory '151500px-menu.exe.sha256') -Encoding ascii
    Write-Host "Built: $menuExe"
} finally { Pop-Location }
