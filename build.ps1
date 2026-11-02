$ErrorActionPreference = 'Stop'
Set-Location $PSScriptRoot
$programFilesX86 = [Environment]::GetEnvironmentVariable('ProgramFiles(x86)')
$vswhere = "$programFilesX86\Microsoft Visual Studio\Installer\vswhere.exe"
$installation = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Tools.x86.x64 -property installationPath
if (!$installation) { throw 'Install Visual Studio with Desktop development with C++.' }
& "$installation\MSBuild\Current\Bin\MSBuild.exe" DyslexiReader.vcxproj /p:Configuration=Release /p:Platform=x64 /p:LinkIncremental=false /v:minimal
if ($LASTEXITCODE) { throw 'Build failed.' }
