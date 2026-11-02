$ErrorActionPreference = 'Stop'
Set-Location (Split-Path -Parent $PSScriptRoot)
$projectRoot = (Get-Location).Path
$target = Join-Path $projectRoot 'installer-smoke'
if (Test-Path -LiteralPath $target) { throw 'Test directory already exists; inspect it before running this test again.' }
& .\tools\InnoSetup\ISCC.exe /Q /DSmokeTest installer\DyslexiReader.iss
if ($LASTEXITCODE) { throw 'Test installer compilation failed.' }
$setupArguments = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART','/TASKS=',('/DIR="' + $target + '"'),('/LOG="' + $projectRoot + '\dist\install-test.log"'))
$process = Start-Process -FilePath .\dist\DyslexiReader-Setup-Test.exe -ArgumentList $setupArguments -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode) { throw "Installation failed: $($process.ExitCode)" }
foreach ($file in @('DyslexiReader.exe','pdfium.dll','PDFium-LICENSE.txt')) {
    $expected = (Get-FileHash -LiteralPath (Join-Path $projectRoot "build\$file")).Hash
    $actual = (Get-FileHash -LiteralPath (Join-Path $target $file)).Hash
    if ($expected -ne $actual) { throw "Installed file differs: $file" }
}
if ((Get-ChildItem -LiteralPath "$target\licenses" -File).Count -ne 18) { throw 'License notices missing.' }
if (!(Test-Path -LiteralPath "$target\unins000.exe")) { throw 'Uninstaller missing.' }
$uninstallArguments = @('/VERYSILENT','/SUPPRESSMSGBOXES','/NORESTART',('/LOG="' + $projectRoot + '\dist\uninstall-test.log"'))
$process = Start-Process -FilePath "$target\unins000.exe" -ArgumentList $uninstallArguments -WindowStyle Hidden -PassThru -Wait
if ($process.ExitCode) { throw "Uninstallation failed: $($process.ExitCode)" }
for ($attempt = 0; $attempt -lt 20 -and (Test-Path -LiteralPath "$target\DyslexiReader.exe"); $attempt++) { Start-Sleep -Milliseconds 250 }
if (Test-Path -LiteralPath "$target\DyslexiReader.exe") { throw 'Uninstall did not remove application.' }
$uninstallKey = 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Uninstall\DyslexiReader.InstallerSmokeTest_is1'
if (Test-Path -LiteralPath $uninstallKey) { throw 'Uninstall registration remains.' }
'PASS: install, executable/DLL hash verification, license notices, uninstaller creation, removal, and uninstall registration cleanup.' | Set-Content dist/installer-test-results.txt
Get-Content dist/installer-test-results.txt
