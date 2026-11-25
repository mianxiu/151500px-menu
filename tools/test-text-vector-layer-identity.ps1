$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$qa = (New-Item -Path (Join-Path $root ('.runtime/vector-layer-identity-' + [guid]::NewGuid().ToString('N'))) -ItemType Directory).FullName
$rasterizer = (Get-Command pdftoppm.exe -ErrorAction Stop).Source
$status = Invoke-RestMethod 'http://127.0.0.1:15151/api/status' -TimeoutSec 3
$process = Get-Process -Name Photoshop
if (@($process).Count -ne 1 -or !$process.MainWindowHandle -or !(Test-Path -LiteralPath $process.Path) -or
    !$status.photoshop.uxp_ready -or $status.photoshop.busy -or $status.stability.active -or $status.menu.visible -or
    (Test-Path -LiteralPath (Join-Path $status.data_dir 'uxp-pending.json'))) {
    throw 'No idle, verified Photoshop session'
}
$com = [Runtime.InteropServices.Marshal]::GetActiveObject('Photoshop.Application.170')
if ($com.Version -ne '24.0.1') { throw 'This fixture targets Photoshop 24.0.1' }
$header = Get-Content -LiteralPath (Join-Path $env:APPDATA 'Adobe/UXP/PluginsStorage/PHSP/24/External/com.15100px.menu.offline-probe/PluginData/probe-last.json') -Raw -Encoding utf8 | ConvertFrom-Json
if ([long]($header.session.Split('-')[0]) -lt ([DateTimeOffset]$process.StartTime).ToUnixTimeMilliseconds()) {
    throw 'Photoshop and UXP session do not match'
}
$editing = $com.DoJavaScript("(function(){var s=stringIDToTypeID,r=new ActionReference();r.putProperty(s('property'),s('modalToolLevel'));r.putEnumerated(s('application'),s('ordinal'),s('targetEnum'));return executeActionGet(r).getInteger(s('modalToolLevel'));})()")
if ([int]$editing -ne 0) { throw 'Photoshop is editing a document now' }

function Write-VectorFixture([string]$path,[string]$components) {
    $stream = "$components rg 8 8 100 56 re f`n"
    $objects = @(
        '<< /Type /Catalog /Pages 2 0 R >>',
        '<< /Type /Pages /Kids [3 0 R] /Count 1 >>',
        '<< /Type /Page /Parent 2 0 R /MediaBox [0 0 144 144] /Resources << >> /Contents 4 0 R >>',
        "<< /Length $($stream.Length) >>`nstream`n${stream}endstream"
    )
    $pdf = "%PDF-1.4`n"
    $offsets = @()
    for ($i=0; $i -lt $objects.Length; $i++) {
        $offsets += ('{0:0000000000} 00000 n ' -f $pdf.Length)
        $pdf += "$($i+1) 0 obj`n$($objects[$i])`nendobj`n"
    }
    $xref = $pdf.Length
    $pdf += "xref`n0 5`n0000000000 65535 f `n" + ($offsets -join "`n") +
        "`ntrailer`n<< /Size 5 /Root 1 0 R >>`nstartxref`n$xref`n%%EOF`n"
    [IO.File]::WriteAllText($path,$pdf,[Text.Encoding]::ASCII)
}
Write-VectorFixture (Join-Path $qa 'source-a.pdf') '1 0 0'
Write-VectorFixture (Join-Path $qa 'source-b.pdf') '0 0 1'
$directory = ConvertTo-Json -InputObject ($qa.Replace('\','/')) -Compress
$script = "var TEST_DIRECTORY=$directory;`n" + [IO.File]::ReadAllText((Join-Path $root 'tools/test-text-vector-layer-identity.jsx'))
$fields = ([string]$com.DoJavaScript($script)).Split(@([char]'|'),8)
if ($fields.Count -ne 8) { throw 'Missing Photoshop fixture result' }
$result = [pscustomobject]@{
    ordinary = $fields[0] -eq 'true'
    filterPreservedEditableText = $fields[1] -eq 'true'
    sharedBefore = $fields[2] -eq 'true'
    sameLayer = $fields[3] -eq 'true'
    peerSameLayer = $fields[4] -eq 'true'
    crossDocumentSharesId = $fields[5] -eq 'true'
    uniqueSameLayer = $fields[6] -eq 'true'
    filter = $fields[7]
}
function Read-VectorColor([string]$name) {
    $base = Join-Path $qa $name
    & $rasterizer -r 72 -singlefile "${base}.pdf" $base
    if ($LASTEXITCODE) { throw 'Could not rasterize exported PDF evidence' }
    $bytes = [IO.File]::ReadAllBytes("${base}.ppm")
    $lineFeeds = 0
    $i = 0
    while ($lineFeeds -lt 3 -and $i -lt $bytes.Length) {
        if ($bytes[$i] -eq 10) { $lineFeeds++ }
        $i++
    }
    if ($lineFeeds -ne 3 -or ($bytes.Length-$i)%3 -ne 0) { throw 'Unexpected PDF raster evidence' }
    $red = 0
    $blue = 0
    for (; $i+2 -lt $bytes.Length; $i+=3) {
        if ($bytes[$i] -gt 200 -and $bytes[$i+1] -lt 120 -and $bytes[$i+2] -lt 120) { $red++ }
        if ($bytes[$i] -lt 120 -and $bytes[$i+1] -lt 120 -and $bytes[$i+2] -gt 200) { $blue++ }
    }
    if ($red -gt 100 -and !$blue) { return 'red' }
    if ($blue -gt 100 -and !$red) { return 'blue' }
    throw "Unexpected raster colors for $name ($red red, $blue blue)"
}
$sharedBefore = Read-VectorColor 'shared-before'
$targetAfter = Read-VectorColor 'target-after'
$peerAfter = Read-VectorColor 'peer-after'
$uniqueAfter = Read-VectorColor 'unique-target-after'
$crossAfter = Read-VectorColor 'cross-after'
if ($sharedBefore -ne 'red' -or $targetAfter -ne 'blue' -or $uniqueAfter -ne 'blue') { throw 'Replacement comparison did not match its fixture' }
$result | Add-Member -NotePropertyName sharedBeforeColor -NotePropertyValue $sharedBefore
$result | Add-Member -NotePropertyName targetAfterColor -NotePropertyValue $targetAfter
$result | Add-Member -NotePropertyName peerAfterColor -NotePropertyValue $peerAfter
$result | Add-Member -NotePropertyName uniqueAfterColor -NotePropertyValue $uniqueAfter
$result | Add-Member -NotePropertyName crossAfterColor -NotePropertyValue $crossAfter
$result = $result | ConvertTo-Json -Compress
[IO.File]::WriteAllText((Join-Path $qa 'report.json'),$result)
$result
$qa
