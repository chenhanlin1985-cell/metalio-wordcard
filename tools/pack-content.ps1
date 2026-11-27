param(
    [Parameter(Mandatory)][string]$SourceDirectory,
    [Parameter(Mandatory)][string]$OutputFile
)
$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
[Console]::OutputEncoding = [Text.UTF8Encoding]::new($false)
[Console]::InputEncoding = [Console]::OutputEncoding
$OutputEncoding = [Console]::OutputEncoding
$packPython = Join-Path $env:LOCALAPPDATA 'Programs/Python/Python310/python.exe'
if (-not (Test-Path -LiteralPath $packPython)) {
    $packPython = (Get-Command python -CommandType Application).Source
}
$packTool = Join-Path $PSScriptRoot '../experiments/pet-voyage/packages.py'
& $packPython $packTool --pack $SourceDirectory $OutputFile
exit $LASTEXITCODE
