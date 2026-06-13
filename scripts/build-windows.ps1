param(
    [string]$Configuration = "Release",
    [string]$Version = "0.1.0"
)

$ErrorActionPreference = "Stop"

$Root = Split-Path -Parent $PSScriptRoot
$BuildDir = Join-Path $Root "build\windows"
$PackageDir = Join-Path $BuildDir "package"
$Exe = Join-Path $BuildDir "ModernScreenshot.exe"
$Source = Join-Path $Root "src\modern_screenshot_win.cpp"
$Zip = Join-Path $BuildDir "ModernScreenshot-windows-x64.zip"

New-Item -ItemType Directory -Force -Path $BuildDir | Out-Null
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null

if (-not (Get-Command cl.exe -ErrorAction SilentlyContinue)) {
    throw "cl.exe was not found. Run this from a Visual Studio Developer PowerShell, or use the GitHub Actions workflow."
}

$CommonArgs = @(
    "/nologo",
    "/std:c++17",
    "/EHsc",
    "/DUNICODE",
    "/D_UNICODE",
    "/DVERSION=`"$Version`"",
    "/Fe:$Exe",
    $Source,
    "/link",
    "/SUBSYSTEM:WINDOWS",
    "user32.lib",
    "gdi32.lib",
    "gdiplus.lib",
    "shell32.lib",
    "ole32.lib",
    "uuid.lib"
)

if ($Configuration -eq "Release") {
    $CompileArgs = @("/O1", "/GL", "/MT") + $CommonArgs + @("/LTCG", "/OPT:REF", "/OPT:ICF")
} else {
    $CompileArgs = @("/Zi", "/Od", "/MTd") + $CommonArgs
}

& cl.exe @CompileArgs
if ($LASTEXITCODE -ne 0) {
    throw "cl.exe failed with exit code $LASTEXITCODE"
}

Remove-Item -Recurse -Force $PackageDir -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force -Path $PackageDir | Out-Null
Copy-Item $Exe (Join-Path $PackageDir "ModernScreenshot.exe")
Copy-Item (Join-Path $Root "README.md") (Join-Path $PackageDir "README.md")
Copy-Item (Join-Path $Root "LICENSE") (Join-Path $PackageDir "LICENSE")

if (Test-Path $Zip) {
    Remove-Item $Zip
}
Compress-Archive -Path (Join-Path $PackageDir "*") -DestinationPath $Zip -Force

Write-Host "Built $Exe"
Write-Host "Packaged $Zip"
