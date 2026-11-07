param(
    [Parameter(Mandatory=$true)][string]$IdfPath,
    [string]$BuildDirectory='',
    [string]$CompilerBin=''
)
$ErrorActionPreference='Stop'
$env:PYTHONUTF8='1'
$env:PYTHONIOENCODING='utf-8'
[Console]::InputEncoding=[Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false)
$OutputEncoding=[Console]::OutputEncoding
if (!(Test-Path -LiteralPath (Join-Path $IdfPath 'export.ps1'))) {throw 'ESP-IDF export.ps1 not found'}
. (Join-Path $IdfPath 'export.ps1')
if ($CompilerBin) {$env:PATH="$CompilerBin;$env:PATH"}
$buildPython=Join-Path $env:IDF_PYTHON_ENV_PATH 'Scripts/python.exe'
if (!(Test-Path -LiteralPath $buildPython)) {throw 'IDF Python environment unavailable'}
& $buildPython -u (Join-Path $PSScriptRoot 'tools/build_preflight.py')
if ($LASTEXITCODE -ne 0) {throw 'UTF-8 preflight failed'}
if (!$BuildDirectory) {$BuildDirectory=Join-Path $PSScriptRoot 'build'}
$BuildDirectory=[IO.Path]::GetFullPath($BuildDirectory)
$defaults=Join-Path $PSScriptRoot 'firmware/sdkconfig.defaults'
$config=Join-Path $BuildDirectory 'sdkconfig'
& $buildPython -u (Join-Path $IdfPath 'tools/idf.py') -C (Join-Path $PSScriptRoot 'firmware') -B $BuildDirectory -DIDF_TARGET=esp32s31 "-DSDKCONFIG=$config" "-DSDKCONFIG_DEFAULTS=$defaults" build
if ($LASTEXITCODE -ne 0) {throw 'Build failed. Do not use an older binary left in the output directory.'}
