param(
    [switch]$SkipBuild,
    [int]$Moves = 2000,
    [int]$Keys = 200,
    [string]$OutputDirectory = ''
)
$ErrorActionPreference = 'Stop'
$menuRoot = Split-Path -Parent $PSScriptRoot
if (-not $OutputDirectory) { $OutputDirectory = Join-Path $menuRoot '.runtime/ime-312/perf' }
New-Item -ItemType Directory -Force -Path $OutputDirectory | Out-Null
$build = Join-Path $OutputDirectory 'build'
if (-not $SkipBuild) { & (Join-Path $PSScriptRoot 'measure-hook-scope.ps1') -OutputDirectory $build | Out-Null }
# .runtime carries a Low mandatory label; an EXE started from there runs Low
# and UIPI blocks synthetic input to Medium Photoshop. Run a copy from the
# user temp directory (Medium) so the PS group uses real SendInput.
$mediumDirectory = Join-Path $env:TEMP 'ime312-hook-measurement'
New-Item -ItemType Directory -Force -Path $mediumDirectory | Out-Null
foreach ($variant in @('baseline','candidate')) {
    Copy-Item -LiteralPath (Join-Path $build "measure-$variant.exe") -Destination (Join-Path $mediumDirectory "measure-$variant.exe") -Force
}

if (-not ('HookMeasureForeground' -as [type])) { Add-Type @'
using System;using System.Text;using System.Runtime.InteropServices;
public class HookMeasureForeground {
    [DllImport("user32.dll")] public static extern IntPtr GetForegroundWindow();
    [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
    [DllImport("user32.dll")] public static extern bool ShowWindow(IntPtr h,int c);
    [DllImport("user32.dll")] public static extern int GetClassName(IntPtr h,StringBuilder s,int n);
    [DllImport("user32.dll")] public static extern uint GetWindowThreadProcessId(IntPtr h,out uint p);
}
'@ }
function Get-WindowInfo([IntPtr]$Handle) {
    $name=New-Object System.Text.StringBuilder 128
    [void][HookMeasureForeground]::GetClassName($Handle,$name,128)
    $windowPid=0;[void][HookMeasureForeground]::GetWindowThreadProcessId($Handle,[ref]$windowPid)
    [pscustomobject]@{handle=$Handle.ToInt64();class=$name.ToString();pid=$windowPid}
}
function Get-PhotoshopRoot {
    $process=Get-Process Photoshop -ErrorAction SilentlyContinue | Select-Object -First 1
    if (-not $process) { return $null }
    return [pscustomobject]@{handle=$process.MainWindowHandle;pid=$process.Id;path=$process.Path}
}
function Activate-Photoshop($root) {
    [void][HookMeasureForeground]::ShowWindow($root.handle,9)
    [void][HookMeasureForeground]::SetForegroundWindow($root.handle)
    $end=[datetime]::UtcNow.AddSeconds(2)
    while ([datetime]::UtcNow -lt $end) {
        $foreground=[HookMeasureForeground]::GetForegroundWindow()
        if ($foreground -eq $root.handle) {
            $info=Get-WindowInfo $foreground
            if ($info.class -eq 'Photoshop' -and $info.pid -eq $root.pid) { return $true }
        }
        Start-Sleep -Milliseconds 50
        [void][HookMeasureForeground]::SetForegroundWindow($root.handle)
    }
    return $false
}

$originalForeground=[HookMeasureForeground]::GetForegroundWindow()
$ps=Get-PhotoshopRoot
$matrix=[System.Collections.Generic.List[object]]::new()
$combinations=@()
foreach ($low in @($true,$false)) {
    foreach ($version in @('baseline','candidate')) {
        foreach ($group in @('placeholder','ps')) {
            $combinations += [pscustomobject]@{low=$low;version=$version;group=$group}
        }
    }
}
foreach ($item in $combinations) {
    $mode=if ($item.low) {'low'} else {'normal'}
    $foregroundOk=$true
    if ($item.group -eq 'ps') {
        $foregroundOk=$ps -and (Activate-Photoshop $ps)
        if (-not $foregroundOk) {
            $matrix.Add([pscustomobject]@{version=$item.version;low=$item.low;group=$item.group;
                ok=$false;error='photoshop-foreground-unverified';json=$null;stdout='';stderr='';elapsed_ms=0})
            continue
        }
    }
    $name="hook-$($item.version)-$mode-$($item.group).json"
    $output=Join-Path $OutputDirectory $name
    $arguments=@()
    if ($item.version -eq 'baseline') { $arguments += '--baseline' }
    if ($item.low) { $arguments += '--low' }
    if ($item.group -eq 'ps') { $arguments += '--photoshop' }
    $arguments += @('--moves',$Moves,'--keys',$Keys,'--output',$output)
    $exe=Join-Path $mediumDirectory "measure-$($item.version).exe"
    $started=[datetime]::UtcNow
    $stdout=& $exe @arguments 2>&1
    $exit=$LASTEXITCODE
    $elapsed=[int]([datetime]::UtcNow-$started).TotalMilliseconds
    $row=[pscustomobject]@{version=$item.version;low=$item.low;group=$item.group;ok=($exit -eq 0);
        error=if($exit -eq 0){$null}else{"exit-$exit"};json=$output;stdout=($stdout -join "`n");
        stderr='';elapsed_ms=$elapsed}
    $matrix.Add($row)
    Start-Sleep -Milliseconds 250
}
if ($originalForeground -ne [IntPtr]::Zero) { [void][HookMeasureForeground]::SetForegroundWindow($originalForeground) }

# Load raw results and compute absolute/percentage changes per comparable pair.
$rows=@()
foreach ($row in $matrix) {
    if (-not $row.json -or -not (Test-Path -LiteralPath $row.json)) { continue }
    $value=Get-Content -LiteralPath $row.json -Raw | ConvertFrom-Json
    $rows += [pscustomobject]@{version=$row.version;low=$row.low;group=$row.group;raw=$value;
        p50_mouse=$value.mouse.p50_us;p95_mouse=$value.mouse.p95_us;max_mouse=$value.mouse.max_us;
        n_mouse=$value.mouse.count;cpu_mouse=$value.mouse.cpu_delta_us;
        p50_key=$value.key.p50_us;p95_key=$value.key.p95_us;max_key=$value.key.max_us;
        n_key=$value.key.count;cpu_key=$value.key.cpu_delta_us;
        integrity=$value.integrity_rid;direct=$value.direct;affinity=$value.affinity_mask;priority=$value.priority_class}
}
function Change($baseline,$candidate) {
    if ($null -eq $baseline -or $baseline -eq 0) { return $null }
    return [math]::Round((([double]$candidate-[double]$baseline)/[double]$baseline)*100,1)
}
$comparisons=@()
foreach ($low in @($true,$false)) {
    foreach ($group in @('placeholder','ps')) {
        $base=$rows | Where-Object { $_.version -eq 'baseline' -and $_.low -eq $low -and $_.group -eq $group } | Select-Object -First 1
        $candidate=$rows | Where-Object { $_.version -eq 'candidate' -and $_.low -eq $low -and $_.group -eq $group } | Select-Object -First 1
        if (-not $base -or -not $candidate) { continue }
        $comparisons += [pscustomobject]@{
            low=$low;group=$group;direct=$candidate.direct
            baseline_p50_mouse=$base.p50_mouse;candidate_p50_mouse=$candidate.p50_mouse
            delta_p50_mouse=[int]$candidate.p50_mouse-[int]$base.p50_mouse;pct_p50_mouse=Change $base.p50_mouse $candidate.p50_mouse
            baseline_p95_mouse=$base.p95_mouse;candidate_p95_mouse=$candidate.p95_mouse
            delta_p95_mouse=[int]$candidate.p95_mouse-[int]$base.p95_mouse;pct_p95_mouse=Change $base.p95_mouse $candidate.p95_mouse
            baseline_max_mouse=$base.max_mouse;candidate_max_mouse=$candidate.max_mouse
            baseline_p50_key=$base.p50_key;candidate_p50_key=$candidate.p50_key
            delta_p50_key=[int]$candidate.p50_key-[int]$base.p50_key;pct_p50_key=Change $base.p50_key $candidate.p50_key
            baseline_p95_key=$base.p95_key;candidate_p95_key=$candidate.p95_key
            delta_p95_key=[int]$candidate.p95_key-[int]$base.p95_key;pct_p95_key=Change $base.p95_key $candidate.p95_key
            baseline_max_key=$base.max_key;candidate_max_key=$candidate.max_key
            baseline_cpu_mouse=$base.cpu_mouse;candidate_cpu_mouse=$candidate.cpu_mouse
            baseline_cpu_key=$base.cpu_key;candidate_cpu_key=$candidate.cpu_key
            n_mouse=$candidate.n_mouse;n_key=$candidate.n_key
            baseline_integrity=$base.integrity;candidate_integrity=$candidate.integrity
            candidate_affinity=$candidate.affinity;candidate_priority=$candidate.priority
            baseline_json=$base.raw.samples_file;candidate_json=$candidate.raw.samples_file
        }
    }
}
$result=[pscustomobject]@{moves=$Moves;keys=$Keys;measurement=$matrix;comparisons=$comparisons;
    original_foreground=$originalForeground.ToInt64();photoshop_pid=if($ps){$ps.pid}else{$null}}
$result | ConvertTo-Json -Depth 8 | Set-Content -LiteralPath (Join-Path $OutputDirectory 'matrix.json') -Encoding utf8
$comparisons | Format-Table -AutoSize | Out-String | Write-Host
$result | ConvertTo-Json -Depth 8
