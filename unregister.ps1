$ErrorActionPreference = 'Stop'
$paths = @(
    'HKCU:\Software\Classes\DyslexiReader.PDF',
    'HKCU:\Software\Classes\Applications\DyslexiReader.exe'
)
foreach ($path in $paths) {
    if (Test-Path -LiteralPath $path) { Remove-Item -LiteralPath $path -Recurse }
}
$openWith = 'HKCU:\Software\Classes\.pdf\OpenWithProgids'
if (Test-Path -LiteralPath $openWith) {
    Remove-ItemProperty -LiteralPath $openWith -Name 'DyslexiReader.PDF' -ErrorAction SilentlyContinue
}
Write-Output 'DyslexiReader registration removed. Choose another PDF default in Windows Settings if needed.'
