param(
    [string]$Library,
    [string]$Catalog,
    [string]$Build
)
$ErrorActionPreference = 'Stop'
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
[Console]::InputEncoding = [Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding = [Console]::InputEncoding
$OutputEncoding = [Console]::OutputEncoding
$root = Split-Path $PSScriptRoot -Parent
foreach ($fontFile in 'font.fontpack', 'learning-kaishu-v3.fontpack') {
    if (!(Test-Path -LiteralPath (Join-Path $root "assets/$fontFile") -PathType Leaf)) {
        throw "Missing assets/$fontFile. See docs/BUILD.md for the fonts from the full release."
    }
}
$source = Join-Path $root 'experiments/pet-voyage'
if (!$Library) { $Library = Join-Path $root 'work/wordcard-content/library' }
if (!$Catalog) { $Catalog = Join-Path $root 'resources/vocabulary' }
Push-Location $PSScriptRoot
try {
    & python ./build_preflight.py
    if ($LASTEXITCODE) { throw 'UTF-8 preflight failed' }
} finally { Pop-Location }
$vs = & 'C:/Program Files (x86)/Microsoft Visual Studio/Installer/vswhere.exe' -latest -products * -property installationPath
$cmake = Join-Path $vs 'Common7/IDE/CommonExtensions/Microsoft/CMake/CMake/bin/cmake.exe'
if (!$Build) { $Build = Join-Path $root 'work/native-learning/build' }
$testFolder = Join-Path $root 'work/story-reader-check'
New-Item -ItemType Directory -Force $testFolder | Out-Null
$fixtures = Join-Path $testFolder 'standard-library'
& python "$source/test-story-format.py" --decks $Library --library $fixtures
if ($LASTEXITCODE) { throw 'Invalid shipping story packs' }
$Library = $fixtures
& node "$source/bundle-device.cjs"
if ($LASTEXITCODE) { throw 'Device bundle failed' }
& $cmake -S "$root/tests/native_learning" -B $build -G 'Visual Studio 17 2022' -A x64
if ($LASTEXITCODE) { throw 'Could not configure native host tests' }
& $cmake --build $build --config Release --target pack_vm native_preview
if ($LASTEXITCODE) { throw 'Native host build failed' }
$oldLibrary = $env:WORDCARD_LIBRARY
try {
    $env:WORDCARD_LIBRARY = $Library
    foreach ($suite in 'navigation-check','book-format-test') {
        $fixture = Join-Path $testFolder ($suite + '.js')
        & node "$source/$suite.cjs" $fixture
        if ($LASTEXITCODE) { throw "Failed $suite" }
        & "$build/Release/pack_vm.exe" $fixture
        if ($LASTEXITCODE) { throw "Failed QuickJS $suite" }
    }
} finally { $env:WORDCARD_LIBRARY = $oldLibrary }
& python "$source/test-device-buttons.py" --exe "$build/Release/native_preview.exe" --library $Library --catalog $Catalog
if ($LASTEXITCODE) { throw 'Story button regression failed' }
& python "$source/test-word-navigation.py" --exe "$build/Release/native_preview.exe" --library $Library --catalog $Catalog
if ($LASTEXITCODE) { throw 'Word-group navigation regression failed' }
& python "$source/test-shared-vocabulary.py" --exe "$build/Release/native_preview.exe" --library $Library --catalog $Catalog
if ($LASTEXITCODE) { throw 'Shared vocabulary regression failed' }
& python "$source/test-story-ledger.py" --exe "$build/Release/native_preview.exe" --library $Library --catalog $Catalog
if ($LASTEXITCODE) { throw 'Story and deck schedule regression failed' }
& python "$source/test-course-ledger.py" --exe "$build/Release/native_preview.exe" --library $Library --catalog $Catalog
if ($LASTEXITCODE) { throw 'Course and deck schedule regression failed' }
& python "$source/test-sense-contract.py" --exe "$build/Release/native_preview.exe" --library $Library --catalog $Catalog
if ($LASTEXITCODE) { throw 'Sense identity or atomic storage regression failed' }
& python "$source/test-learned-history.py" --exe "$build/Release/native_preview.exe" --library $Library --catalog $Catalog
if ($LASTEXITCODE) { throw 'Learned word history regression failed' }
& python "$source/test-vocabulary-books.py" --exe "$build/Release/native_preview.exe" --library $Library --catalog $Catalog
if ($LASTEXITCODE) { throw 'Vocabulary story format regression failed' }
