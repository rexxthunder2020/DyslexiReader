param([string]$CompilerPath, [switch]$SkipAppBuild)
$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
if (!$SkipAppBuild) { & .\build.ps1 }
if (!$CompilerPath) {
    $candidates = @(
        (Join-Path $PSScriptRoot 'tools\InnoSetup\ISCC.exe'),
        (Join-Path ([Environment]::GetEnvironmentVariable('ProgramFiles(x86)')) 'Inno Setup 6\ISCC.exe'),
        (Join-Path $env:ProgramFiles 'Inno Setup 7\ISCC.exe')
    )
    $CompilerPath = $candidates | Where-Object { Test-Path -LiteralPath $_ } | Select-Object -First 1
}
if (!$CompilerPath) { throw 'Install Inno Setup 6.3 or newer, or pass -CompilerPath to ISCC.exe.' }
& $CompilerPath installer\DyslexiReader.iss
if ($LASTEXITCODE) { throw 'Installer compilation failed.' }
Write-Output "Installer: $PSScriptRoot\dist\DyslexiReader-Setup-1.1.0-x64.exe"

