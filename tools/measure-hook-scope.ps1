param(
    [string]$BaselineRevision = '24e938b',
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$menuRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $menuRoot '.runtime/ime-312/perf/build' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$menuCompiler = (Get-Command g++.exe -ErrorAction Stop).Source
$tool = (Join-Path $PSScriptRoot 'measure-hook-scope.cpp').Replace('\', '/')
$candidate = (Join-Path $menuRoot 'native/HotkeyHost.cpp').Replace('\', '/')
$baselineSource = Join-Path $OutputDirectory 'HotkeyHost-0.4.311.cpp'

# The baseline revision is instrumented only in this ignored copy. The patch
# adds the same pre-CallNextHookEx timing call the current source carries under
# HOTKEY_MEASURE; no decision, scope or input behavior is changed.
$old = git -C $menuRoot show "${BaselineRevision}:native/HotkeyHost.cpp"
if ($LASTEXITCODE -ne 0) { throw "Cannot read $BaselineRevision native/HotkeyHost.cpp" }
$oldText = ($old -join "`n") + "`n"
$timingOld = @'
struct CallbackTiming {
    ULONGLONG start=microseconds();
    ~CallbackTiming() {
'@
$timingNew = @'
#ifdef HOTKEY_MEASURE
extern void measureHookSample(unsigned long long,int);
static int measurementKind=0;
static bool measureDirectMode=false;
#endif
struct CallbackTiming {
    ULONGLONG start=microseconds();
    ULONGLONG elapsed() const {return microseconds()-start;}
    ~CallbackTiming() {
'@
if (-not $oldText.Contains($timingOld)) { throw 'Baseline CallbackTiming shape changed.' }
$oldText = $oldText.Replace($timingOld, $timingNew)
$trigger = @'
#ifdef HOTKEY_MEASURE
    measureHookSample(timing.elapsed(),measurementKind);
    if(measureDirectMode)return 0;
#endif
    return CallNextHookEx(nullptr,code,message,data);
'@
$keyboard = [regex]::Replace($oldText, '(?m)^    return CallNextHookEx\(nullptr,code,message,data\);\r?\n\}\r?\nstatic LRESULT CALLBACK pointer', $trigger + '}' + "`n" + 'static LRESULT CALLBACK pointer', 1)
$pointer = [regex]::Replace($keyboard, '(?m)^    return CallNextHookEx\(nullptr,code,message,data\);\r?\n\}\r?\nstatic void stop\(\)', $trigger + '}' + "`n" + 'static void stop()', 1)
if ($pointer -eq $keyboard -or $pointer -eq $oldText) { throw 'Baseline hook return shape changed.' }
[System.IO.File]::WriteAllText($baselineSource, $pointer, [System.Text.UTF8Encoding]::new($false))
$baseline = $baselineSource.Replace('\', '/')
@"
#define HOTKEY_MEASURE 1
#define MEASURE_BASELINE 1
#define HOTKEY_MEASURE_SOURCE "$baseline"
#include "$tool"
"@ | Set-Content -LiteralPath (Join-Path $OutputDirectory 'baseline.cpp') -Encoding utf8
@"
#define HOTKEY_MEASURE 1
#define HOTKEY_MEASURE_SOURCE "$candidate"
#include "$tool"
"@ | Set-Content -LiteralPath (Join-Path $OutputDirectory 'candidate.cpp') -Encoding utf8
$include = Join-Path $menuRoot 'native'
foreach ($variant in @('baseline', 'candidate')) {
    $exe = Join-Path $OutputDirectory ("measure-$variant.exe")
    & $menuCompiler -std=c++17 -O2 -Wall -Wextra -static (Join-Path $OutputDirectory "$variant.cpp") -o $exe -luser32 -ladvapi32 "-I$include"
    if ($LASTEXITCODE -ne 0) { throw "Hook measurement build failed: $variant" }
}
$result = [ordered]@{
    baseline_revision = $BaselineRevision
    baseline_source = $baselineSource
    baseline_source_sha256 = (Get-FileHash -LiteralPath $baselineSource -Algorithm SHA256).Hash
    candidate_source = $candidate
    candidate_source_sha256 = (Get-FileHash -LiteralPath $candidate -Algorithm SHA256).Hash
    baseline_exe = Join-Path $OutputDirectory 'measure-baseline.exe'
    baseline_exe_sha256 = (Get-FileHash -LiteralPath (Join-Path $OutputDirectory 'measure-baseline.exe') -Algorithm SHA256).Hash
    candidate_exe = Join-Path $OutputDirectory 'measure-candidate.exe'
    candidate_exe_sha256 = (Get-FileHash -LiteralPath (Join-Path $OutputDirectory 'measure-candidate.exe') -Algorithm SHA256).Hash
}
$result | ConvertTo-Json | Set-Content -LiteralPath (Join-Path $OutputDirectory 'build.json') -Encoding utf8
$result | ConvertTo-Json
