$ErrorActionPreference = 'Stop'
$root = Split-Path -Parent $PSScriptRoot
$version = '1.0.2903.40'
$checksum = 'EF128016DD1E51C59178C827ED5B8AA3322C57AFA8675D930F8109505542AD74'
$cache = New-Item -ItemType Directory -Force -Path "$root/.runtime/webview2-sdk"
$zip = Join-Path $cache.FullName "microsoft.web.webview2.$version.zip"
if (!(Test-Path -LiteralPath $zip)) {
    $source = "https://api.nuget.org/v3-flatcontainer/microsoft.web.webview2/$version/microsoft.web.webview2.$version.nupkg"
    Invoke-WebRequest $source -OutFile "$zip.partial" -TimeoutSec 60
    if ((Get-FileHash -LiteralPath "$zip.partial").Hash -ne $checksum) { throw 'WebView2 SDK checksum mismatch.' }
    Move-Item -LiteralPath "$zip.partial" -Destination $zip
}
if ((Get-FileHash -LiteralPath $zip).Hash -ne $checksum) { throw 'Cached WebView2 SDK checksum mismatch.' }
Expand-Archive -LiteralPath $zip -DestinationPath "$cache/$version" -Force

$sdk = "$cache/$version"
$target = (New-Item -ItemType Directory -Force -Path "$root/native/control").FullName
foreach ($name in 'Core', 'WinForms') {
    Copy-Item -LiteralPath "$sdk/lib/net462/Microsoft.Web.WebView2.$name.dll" -Destination $target -Force
}
Copy-Item -LiteralPath "$sdk/build/native/x64/WebView2Loader.dll" -Destination $target -Force
Copy-Item -LiteralPath "$sdk/LICENSE.txt" -Destination "$target/WebView2-LICENSE.txt" -Force
Copy-Item -LiteralPath "$sdk/NOTICE.txt" -Destination "$target/WebView2-NOTICE.txt" -Force
$compiler = Join-Path $env:WINDIR 'Microsoft.NET/Framework64/v4.0.30319/csc.exe'
& $compiler /nologo /optimize+ /target:winexe /platform:x64 /codepage:65001 "/out:$target\ControlHost.exe" `
    "/win32icon:$root\frontend\public\icon.ico" '/reference:System.Windows.Forms.dll' '/reference:System.Drawing.dll' `
    "/reference:$target\Microsoft.Web.WebView2.Core.dll" "/reference:$target\Microsoft.Web.WebView2.WinForms.dll" `
    "$root\native\ControlHost.cs"
if ($LASTEXITCODE -ne 0) { throw 'WebView2 management host build failed.' }
