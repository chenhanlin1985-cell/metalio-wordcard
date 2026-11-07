param([Parameter(Mandatory=$true)][ValidatePattern('^[A-Za-z0-9_-]+$')][string]$Run)
$ErrorActionPreference='Stop'
$r=Split-Path $PSScriptRoot -Parent;$w=Join-Path $r ('work/sd-diagnostics-tests/'+$Run)
if(Test-Path -LiteralPath $w){throw 'Use fresh test directory'}
$null=New-Item -ItemType Directory -Path $w
. "$r/tools/sd-diagnostics/diagnostic-core.ps1"
$script:checks=0
function Assert($Condition,$Label){if(!$Condition){throw ('FAIL '+$Label)};$script:checks++}
function Envelope($Value){$d=$Value|ConvertTo-Json -Depth 20 -Compress;return (@{data=$d;checksum=[WordcardDiagIO]::Fnv($d)}|ConvertTo-Json -Compress)+"`n"}
$nativeEncoding=[WordcardDiagIO]::NativeDiskEncoding()
Assert ($nativeEncoding.CodePage -gt 0) 'native OEM codepage is available'
$nativeSample='Windows filesystem check'
if($nativeEncoding.CodePage -eq 936 -or $nativeEncoding.CodePage -eq 65001){$nativeSample='Windows 已扫描文件系统并且没有发现问题。'}
Assert ($nativeEncoding.GetString($nativeEncoding.GetBytes($nativeSample)) -ceq $nativeSample) 'native Chinese stdout roundtrip'
Assert ($nativeEncoding.DecoderFallback -is [Text.DecoderExceptionFallback]) 'native decoding errors are not suppressed'
$root=Join-Path $w 'card';$base=Join-Path $root 'metalio/e-ink/wordcards';$store=Join-Path $base 'learning-web/vocabulary'
$null=New-Item -ItemType Directory -Path $store -Force;$null=New-Item -ItemType Directory -Path (Join-Path $base 'decks')
$state=Join-Path $store 'state.json';$events=Join-Path $store 'events.ndjson';$wordlist=Join-Path $base 'my-words.txt'
[IO.File]::WriteAllText($state,(Envelope @{state=@{revision=0;privateWord='SECRET_LEARNING_WORD'};journalOffset=0}))
[IO.File]::WriteAllText($events,(Envelope @{revision=1;patch=@()}))
[IO.File]::WriteAllText($wordlist,"appointment`nfirmware`n")
$before=[WordcardDiagIO]::HashFile($state);$originalEvents=[IO.File]::ReadAllText($events)
$result=Invoke-StorageDiagnosis $root
Assert ($result.probes.Count -eq 4) 'four relevant directories'
Assert (@($result.probes|Where-Object{!$_.passed -or !$_.cleaned}).Count -eq 0) 'all probe operations and cleanup'
foreach($p in $result.probes){Assert ($p.completed.Count -eq 7) 'create/write/flush/append/readback/rename/delete'}
Assert ($result.progressUnchanged -and $result.unverifiedFiles -eq 0) 'original files unchanged'
Assert ($result.journal.status -eq 'envelopes-and-sequence-ok' -and $result.journal.recordsChecked -eq 1) 'valid journal'
Assert ($result.wordList.expectedPresent) 'wordlist path'
Assert ([WordcardDiagIO]::HashFile($state) -ceq $before) 'state exact bytes preserved'
Assert (@(Get-ChildItem -LiteralPath $root -Recurse -Force -Filter '.__wcdiag_*').Count -eq 0) 'no scratch leak'
Assert (!(($result|ConvertTo-Json -Depth 20) -match 'SECRET_LEARNING_WORD|appointment|firmware')) 'no private content exported'
$readOnly=Invoke-StorageDiagnosis $root -ReadOnly
Assert ($readOnly.probes.Count -eq 0 -and $readOnly.progressUnchanged) 'read-only mode'
[IO.File]::SetAttributes($events,[IO.FileAttributes]::ReadOnly)
$readonlyResult=Invoke-StorageDiagnosis $root
$findings=@(Get-StorageFindings $readonlyResult)
Assert ($findings.code -contains 'record-readonly') 'read-only record identified even if scratch succeeds'
Assert ($readonlyResult.progressUnchanged) 'read-only attributes preserved'
[IO.File]::SetAttributes($events,[IO.FileAttributes]::Normal)
$lock=[IO.FileStream]::new($events,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::None)
try{
    $locked=Invoke-StorageDiagnosis $root -ReadOnly
    Assert ($locked.unverifiedFiles -gt 0 -and $null -eq $locked.progressUnchanged) 'locked record is not claimed verified'
    Assert (@(Get-StorageFindings $locked).code -contains 'file-access-error') 'sharing denial identified'
}finally{$lock.Dispose()}
$failedProbe=[WordcardDiagIO]::Probe($root,(Join-Path $base 'does-not-exist'))
Assert (!$failedProbe.passed -and $failedProbe.stage -eq 'create' -and $failedProbe.cleaned) 'probe failure names exact stage and cleans own file'
[IO.File]::AppendAllText($events,'{"data":')
$tail=Test-Journal $root $store
Assert ($tail.status -eq 'journal-incomplete-tail') 'partial journal detected'
Assert ([IO.File]::ReadAllText($events).EndsWith('{"data":')) 'partial journal not repaired'
[IO.File]::WriteAllText($events,(Envelope @{revision=7;patch=@()}))
Assert ((Test-Journal $root $store).status -eq 'journal-sequence-invalid') 'revision gap detected'
[IO.File]::WriteAllText($events,$originalEvents.Replace('"checksum":','"checksum":0,"oldChecksum":'))
Assert ((Test-Journal $root $store).status -eq 'journal-invalid-or-unreadable') 'bad checksum detected'
[IO.File]::WriteAllText($events,$originalEvents)
$originalState=[IO.File]::ReadAllText($state);[IO.File]::WriteAllText($state,'{"bad":true}')
Assert ((Test-Journal $root $store).status -eq 'checkpoint-invalid') 'bad checkpoint detected'
[IO.File]::WriteAllText($state,$originalState)
[IO.File]::Move($wordlist,(Join-Path $root 'my-words.txt'))
$wrong=Invoke-StorageDiagnosis $root -ReadOnly
Assert (@(Get-StorageFindings $wrong).code -contains 'wordlist-wrong-location') 'wrong wordlist location'
[IO.File]::WriteAllBytes($wordlist,[byte[]]@(255,254,0,0))
$badUtf=Invoke-StorageDiagnosis $root -ReadOnly
Assert ($badUtf.wordList.formatStatus -eq 'invalid-utf8-or-unreadable') 'bad UTF8 detected'
Assert (@(Get-StorageFindings $badUtf).code -contains 'wordlist-format') 'bad encoding explanation'
[IO.File]::WriteAllText($wordlist,'')
Assert ((Invoke-StorageDiagnosis $root -ReadOnly).wordList.formatStatus -eq 'empty-file') 'empty list'
[IO.File]::WriteAllBytes($wordlist,[byte[]]::new(32769))
Assert ((Invoke-StorageDiagnosis $root -ReadOnly).wordList.formatStatus -eq 'over-32-KiB') 'oversized list'
[IO.File]::WriteAllText($wordlist,"appointment`nfirmware`n")
$dirCase=Join-Path $w 'directory-collision';$dirBase=Join-Path $dirCase 'metalio/e-ink/wordcards';$null=New-Item -ItemType Directory -Path $dirBase -Force
[IO.File]::WriteAllText((Join-Path $dirBase 'learning-state-v2.csv'),'fixture')
[IO.File]::WriteAllText((Join-Path $dirBase 'learning-web'),'wrong-path-type')
Assert (@(Get-StorageFindings (Invoke-StorageDiagnosis $dirCase -ReadOnly)).code -contains 'directory-path-error') 'file blocks directory'
$empty=Join-Path $w 'unknown-card';$null=New-Item -ItemType Directory -Path $empty
$rejected=$false;try{Invoke-StorageDiagnosis $empty|Out-Null}catch{$rejected=$true}
Assert $rejected 'unrecognized drive rejected'
$rejected=$false;try{[WordcardDiagIO]::SafePath($root,(Join-Path $w 'outside.txt'))}catch{$rejected=$true}
Assert $rejected 'outside path rejected'
$outside=Join-Path $w 'external';$null=New-Item -ItemType Directory -Path $outside
$junction=Join-Path $base 'junction';$null=New-Item -ItemType Junction -Path $junction -Target $outside
$rejected=$false;try{[WordcardDiagIO]::SafePath($root,(Join-Path $junction 'record.txt'))}catch{$rejected=$true}
Assert $rejected 'junction rejected'
$fixture=[pscustomobject]@{volume=@{freeBytes=0;diskReadOnly=$true;fileSystem='exFAT'};files=@();probes=@();journal=$null;progressUnchanged=$true}
$f=@(Get-StorageFindings $fixture)
Assert ($f.code -contains 'low-space' -and $f.code -contains 'disk-readonly' -and $f.code -contains 'filesystem-check') 'volume causes'
foreach($code in @(2,5,13,23,24,28,30,116)){
    $f=@(Get-DeviceFindings @("SD 未能写入（错误码 $code）") 'a' 'a')
    Assert ($f.code -contains ('device-errno-'+$code)) "device errno $code"
}
$f=@(Get-DeviceFindings @('SD mount: ESP_OK','没有错误') 'same' 'same')
Assert (!($f.code -contains 'device-sd-transport')) 'successful mount not a fault'
$f=@(Get-DeviceFindings @('SD 无法写入') 'a' 'a')
Assert ($f.code -contains 'missing-low-level-error') 'old firmware uncertainty'
$f=@(Get-DeviceFindings @('sdprobe transaction ret=107 cmd_error=107') 'a' 'a')
Assert ($f.code -contains 'device-sd-transport') 'transport fault'
Write-DiagJson (Join-Path $w 'checks.json') @{passed=$true;checks=$script:checks;syntheticFaultFixtures=$true;physicalSdFailureSimulated=$false}
Write-Output ('PASS '+$script:checks+' checks: '+$w)
