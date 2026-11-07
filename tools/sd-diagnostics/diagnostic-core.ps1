$ErrorActionPreference='Stop'
if(!('WordcardDiagIO' -as [type])) { Add-Type -Path (Join-Path $PSScriptRoot 'DiagIO.cs') }
function Write-DiagJson($Path,$Value) {
    [IO.File]::WriteAllText($Path,($Value|ConvertTo-Json -Depth 30),[Text.UTF8Encoding]::new($false))
}
function New-Finding($Code,$Certainty,$Message,$Next) { [pscustomobject]@{code=$Code;certainty=$Certainty;message=$Message;next=$Next} }
function Read-FileFact([string]$Root,[string]$Path) {
    $r=[ordered]@{path=$Path.Substring($Root.TrimEnd('\').Length).TrimStart('\').Replace('\','/');exists=$false;readable=$false;sha256=$null;bytes=$null;attributes=$null;error=$null}
    try {
        [WordcardDiagIO]::SafePath($Root,$Path)
        if(!(Test-Path -LiteralPath $Path)){return [pscustomobject]$r}
        $item=Get-Item -LiteralPath $Path -Force;$r.exists=$true;$r.attributes=$item.Attributes.ToString()
        if($item.PSIsContainer){$r.error='expected-file-is-directory';return [pscustomobject]$r}
        $r.bytes=$item.Length
        if($item.Length -gt 64MB){$r.error='size-limit-not-hashed';return [pscustomobject]$r}
        $r.sha256=[WordcardDiagIO]::HashFile($Path);$r.readable=$true
    } catch {$r.error='read-failed-win32-'+[WordcardDiagIO]::ErrorCode($_.Exception)}
    [pscustomobject]$r
}
function Test-Envelope([string]$Raw) {
    $e=$Raw|ConvertFrom-Json
    if($null -eq $e.data -or $null -eq $e.checksum){throw 'missing-envelope'}
    if([uint64]$e.checksum -ne [uint64][WordcardDiagIO]::Fnv([string]$e.data)){throw 'checksum-mismatch'}
    return ($e.data|ConvertFrom-Json)
}
function Test-Journal([string]$Root,[string]$Store) {
    $r=[ordered]@{status='not-present';checkpoint=$null;journal=$null;recordsChecked=0;replayPerformed=$false;lastRevision=$null}
    $p=Join-Path $Store 'state.json';$backup=Join-Path $Store 'state.json.bak';$journal=Join-Path $Store 'events.ndjson'
    foreach($x in @($p,$backup,$journal)){[WordcardDiagIO]::SafePath($Root,$x)}
    if(!(Test-Path -LiteralPath $p) -and (Test-Path -LiteralPath $backup)){$p=$backup}
    if(!(Test-Path -LiteralPath $p)) {if(Test-Path -LiteralPath $journal){$r.status='checkpoint-missing-with-journal'};return [pscustomobject]$r}
    $r.checkpoint=[IO.Path]::GetFileName($p)
    try {
        if((Get-Item -LiteralPath $p).Length -gt 16MB){$r.status='checkpoint-too-large-not-checked';return [pscustomobject]$r}
        $frame=Test-Envelope ([IO.File]::ReadAllText($p,[Text.UTF8Encoding]::new($false,$true)))
        if($null -eq $frame.state -or $null -eq $frame.journalOffset){throw 'checkpoint-fields-missing'}
        $revision=[long]$frame.state.revision;$offset=[long]$frame.journalOffset;$r.lastRevision=$revision
    } catch {$r.status='checkpoint-invalid';return [pscustomobject]$r}
    if(!(Test-Path -LiteralPath $journal)) {$r.status=if($offset -eq 0){'checkpoint-ok-no-journal'}else{'journal-missing-for-offset'};return [pscustomobject]$r}
    $r.journal='events.ndjson'
    $fs=$null
    try {
        $fs=[IO.FileStream]::new($journal,[IO.FileMode]::Open,[IO.FileAccess]::Read,[IO.FileShare]::Read)
        if($offset -lt 0 -or $offset -gt $fs.Length){$r.status='journal-offset-invalid';return [pscustomobject]$r}
        if($fs.Length-$offset -gt 16MB){$r.status='journal-too-large-not-checked';return [pscustomobject]$r}
        if($offset -gt 0){$null=$fs.Seek($offset-1,[IO.SeekOrigin]::Begin);if($fs.ReadByte() -ne 10){$r.status='journal-offset-not-line-boundary';return [pscustomobject]$r}}
        $null=$fs.Seek($offset,[IO.SeekOrigin]::Begin)
        $buf=[byte[]]::new([int]($fs.Length-$offset));$read=0
        while($read -lt $buf.Length){$n=$fs.Read($buf,$read,$buf.Length-$read);if(!$n){throw 'short-read'};$read+=$n}
        $raw=[Text.UTF8Encoding]::new($false,$true).GetString($buf)
        $lines=$raw.Split("`n");$limit=$lines.Length-1
        for($i=0;$i -lt $limit;$i++){
            $record=Test-Envelope ($lines[$i].TrimEnd("`r"))
            if([long]$record.revision -ne $revision+1){$r.status='journal-sequence-invalid';return [pscustomobject]$r}
            $revision=[long]$record.revision;$r.recordsChecked++;$r.lastRevision=$revision
        }
        $r.status=if($raw.Length -and !$raw.EndsWith("`n")){'journal-incomplete-tail'}else{'envelopes-and-sequence-ok'}
    } catch {$r.status='journal-invalid-or-unreadable'}finally{if($fs){$fs.Dispose()}}
    [pscustomobject]$r
}
function Get-StorageFindings($Result) {
    $f=[Collections.Generic.List[object]]::new()
    if($Result.volume -and $Result.volume.freeBytes -ne $null -and $Result.volume.freeBytes -lt 1MB){$f.Add((New-Finding 'low-space' 'observed' 'SD 剩余可用空间不足 1 MB，可能导致保存失败。' '先备份，清理确认可重建的资源或换卡；不要删除学习记录。'))}
    if($Result.volume -and $Result.volume.diskReadOnly -eq $true){$f.Add((New-Finding 'disk-readonly' 'observed' 'Windows 报告此磁盘处于只读状态。' '区分读卡器锁定、系统只读策略或卡损坏；先备份，不自动解除或格式化。'))}
    if($Result.volume -and $Result.volume.fileSystem -and $Result.volume.fileSystem -notin @('FAT','FAT32')){$f.Add((New-Finding 'filesystem-check' 'observed' ('当前文件系统为 '+$Result.volume.fileSystem+'；当前固件构建使用 FAT，未开启 exFAT。') '先保留全部原文件；不要在本工具里格式化。'))}
    foreach($x in $Result.files){
        $isRecord=$x.path -match '(^|/)(state\.json|events\.ndjson|profile|course-state|day|progress-v|learning-state|guided-state|custom-my-vocabulary)'
        if($isRecord -and $x.exists -and $x.attributes -match 'ReadOnly'){$f.Add((New-Finding 'record-readonly' 'observed' ('记录文件有只读属性：'+$x.path) '此属性可能阻止写入或备份轮转；先保存诊断和备份，未自动修改属性。'))}
        if($x.error -and $x.error -ne 'size-limit-not-hashed'){$f.Add((New-Finding 'file-access-error' 'observed' ($x.path+'：'+$x.error) '检查对应路径/读取权限/介质；不覆盖原文件。'))}
    }
    foreach($x in $Result.directories){if($x.kind -in @('file-instead-of-directory','reparse-or-access-error')){$f.Add((New-Finding 'directory-path-error' 'observed' ('目录路径异常：'+$x.path+'，'+$x.kind) '同名普通文件或重解析路径会阻止创建记录；先备份确认，不自动删除或更名。'))}}
    foreach($x in $Result.probes){if(!$x.passed){
        $reason=switch([int]$x.win32){5{'访问被拒绝：需区分文件权限、只读和安全软件。'}19{'介质写保护。'}21{'设备未就绪。'}23{'数据 CRC 错误。'}32{'文件被其他程序占用。'}112{'磁盘空间不足。'}1117{'I/O 设备错误。'}1392{'文件或目录损坏、无法读取。'}default{'请结合 Windows 错误码和设备日志进一步判断。'}}
        $f.Add((New-Finding 'write-probe-failed' 'observed' ('临时文件测试在 '+$x.directory+' 的 '+$x.stage+' 步骤失败，Windows 错误码 '+$x.win32+'：'+$reason) '仅能确认本次失败的环节；不会直接归因于坏卡。'))
    }}
    if($Result.journal -and $Result.journal.status -notin @('not-present','checkpoint-ok-no-journal','envelopes-and-sequence-ok')){$f.Add((New-Finding 'progress-file-check' 'observed' ('进度文件检查：'+$Result.journal.status) '原文件保持不变，需依据快照/日志继续诊断，不清空进度。'))}
    if($Result.wordList -and !$Result.wordList.expectedPresent -and $Result.wordList.alternativePresent){$f.Add((New-Finding 'wordlist-wrong-location' 'observed' '在其他常见位置发现词表，但固件规定路径下没有 my-words.txt。' '复制到 metalio/e-ink/wordcards/my-words.txt；若已导入成功，继续学习不需要重新导入。'))}
    if($Result.wordList -and $Result.wordList.formatStatus -and $Result.wordList.formatStatus -notin @('not-present','utf8-nonempty')){$f.Add((New-Finding 'wordlist-format' 'observed' ('词表检查：'+$Result.wordList.formatStatus) '只检查了大小和UTF-8文本，不修改词表，也未重新导入。'))}
    if($Result.progressUnchanged -eq $false){$f.Add((New-Finding 'files-changed-during-diagnosis' 'observed' '诊断前后有记录/词表变化，不能宣称数据保持不变。' '停止写入，检查是否有其他程序同时访问；工具没有恢复或覆盖任何快照。'))}
    if(!$f.Count){$f.Add((New-Finding 'no-host-fault-found' 'limited' '本次电脑侧检查未发现明确原因；小文件读写通过不代表卡的全部容量、设备侧句柄和 SD 时序正常。' '同时提供设备端报告；若只有设备失败，继续查设备端 errno、文件句柄、SDMMC 错误。'))}
    return @($f.ToArray())
}
function Invoke-StorageDiagnosis([string]$Root,[switch]$ReadOnly) {
    $Root=[IO.Path]::GetFullPath($Root).TrimEnd('\');$base=Join-Path $Root 'metalio/e-ink/wordcards'
    [WordcardDiagIO]::SafePath($Root,$base)
    if(!(Test-Path -LiteralPath $base -PathType Container)){throw 'No metalio/e-ink/wordcards directory. Refusing writes to an unidentified drive.'}
    $markers=@('dictionary/index.bin','vocabulary/catalog.json','learning-web/vocabulary/state.json','learning-state-v2.csv')
    if(!($markers|Where-Object {Test-Path -LiteralPath (Join-Path $base $_) -PathType Leaf})){throw 'No wordcard resource/progress marker. Refusing probe.'}
    $paths=[Collections.Generic.HashSet[string]]::new([StringComparer]::OrdinalIgnoreCase)
    foreach($n in @('my-words.txt','my-words.txt.txt','progress-v1.csv','learning-state-v2.csv','guided-state-v1.txt','guided-state-v2.txt','guided-state-v3.txt','decks/custom-my-vocabulary.csv','learning-web/vocabulary/state.json','learning-web/vocabulary/events.ndjson')){
        $p=Join-Path $base $n;$null=$paths.Add($p)
        if($n -ne 'my-words.txt' -and $n -ne 'my-words.txt.txt'){foreach($s in @('.bak','.tmp')){$null=$paths.Add($p+$s)}}
    }
    foreach($n in @('my-words.txt','my-words.txt.txt','metalio/e-ink/wordcards/learning-web/my-words.txt')){$null=$paths.Add((Join-Path $Root $n))}
    foreach($dir in @((Join-Path $base 'learning-web'),(Join-Path $base 'learning-web/vocabulary'))){
        [WordcardDiagIO]::SafePath($Root,$dir)
        if(Test-Path -LiteralPath $dir -PathType Container){foreach($f in Get-ChildItem -LiteralPath $dir -File -Force){if($f.Length -le 64MB){$null=$paths.Add($f.FullName)}}}
    }
    $before=@(foreach($p in $paths){Read-FileFact $Root $p})
    $probes=@();$journal=$null;$directories=@()
    foreach($dir in @($base,(Join-Path $base 'learning-web'),(Join-Path $base 'learning-web/vocabulary'),(Join-Path $base 'decks'))){
        $kind='missing'
        try{[WordcardDiagIO]::SafePath($Root,$dir);if(Test-Path -LiteralPath $dir){$kind=if(Test-Path -LiteralPath $dir -PathType Container){'directory'}else{'file-instead-of-directory'}}}catch{$kind='reparse-or-access-error'}
        $directories+=,[pscustomobject]@{path=$dir.Substring($Root.Length).TrimStart('\').Replace('\','/');kind=$kind}
    }
    try{$journal=Test-Journal $Root (Join-Path $base 'learning-web/vocabulary')}catch{$journal=[pscustomobject]@{status='path-or-io-error'}}
    if(!$ReadOnly){foreach($dir in @($base,(Join-Path $base 'learning-web'),(Join-Path $base 'learning-web/vocabulary'),(Join-Path $base 'decks'))){
        if(Test-Path -LiteralPath $dir -PathType Container){$probes+=,[WordcardDiagIO]::Probe($Root,$dir)}
    }}
    $after=@(foreach($p in $paths){Read-FileFact $Root $p})
    $unchanged=$true;$verified=0;$unknown=0
    for($i=0;$i -lt $before.Count;$i++){
        $b=$before[$i];$a=$after[$i]
        if($b.exists -ne $a.exists -or $b.sha256 -cne $a.sha256 -or $b.attributes -cne $a.attributes){$unchanged=$false}
        if($b.exists){if($b.readable -and $a.readable){$verified++}else{$unknown++}}
    }
    $expected=Join-Path $base 'my-words.txt'
    $formatStatus='not-present'
    if(Test-Path -LiteralPath $expected -PathType Leaf){
        try{
            [WordcardDiagIO]::SafePath($Root,$expected);$size=(Get-Item -LiteralPath $expected).Length
            if($size -gt 32768){$formatStatus='over-32-KiB'}else{
                $wordText=[Text.UTF8Encoding]::new($false,$true).GetString([IO.File]::ReadAllBytes($expected)).Trim([char]0xfeff).Trim()
                $formatStatus=if($wordText.Length){'utf8-nonempty'}else{'empty-file'}
            }
        }catch{$formatStatus='invalid-utf8-or-unreadable'}
    }
    $proof=if(!$unchanged){$false}elseif($unknown -gt 0){$null}else{$true}
    [pscustomobject]@{mode='sd';readOnly=[bool]$ReadOnly;files=$before;directories=$directories;probes=$probes;journal=$journal;progressUnchanged=$proof;filesVerified=$verified;unverifiedFiles=$unknown;wordList=@{formatStatus=$formatStatus;expectedPresent=(Test-Path -LiteralPath $expected -PathType Leaf);alternativePresent=(@('my-words.txt','my-words.txt.txt','metalio/e-ink/wordcards/my-words.txt.txt','metalio/e-ink/wordcards/learning-web/my-words.txt')|Where-Object{Test-Path -LiteralPath (Join-Path $Root $_) -PathType Leaf}).Count -gt 0}}
}
function Get-DeviceFindings([string[]]$Lines,$Before,$After) {
    $all=$Lines -join "`n";$f=[Collections.Generic.List[object]]::new()
    foreach($m in [regex]::Matches($all,'错误码\s*(\d+)')) {
        $code=[int]$m.Groups[1].Value
        $reason=switch($code){
            2 {'指定路径不存在：目录/文件缺失，也可能 SD 已掉线。'}
            5 {'设备底层 I/O 错误；需结合 sdmmc/sdprobe 日志，不能仅凭此判坏卡。'}
            13 {'访问被拒绝。当前 IDF 会把 FAT 只读、锁定和其他拒绝合并为此码，不能直接断定坏卡。'}
            23 {'文件打开数量达到限制；优先查固件句柄使用。'}
            24 {'文件描述符数量达到限制；优先查固件句柄使用。'}
            28 {'没有可用空间，可能是数据或目录分配空间耗尽。'}
            30 {'只读文件系统。'}
            default {'需结合固件工具链 errno 定义进一步定位，不能直接套用 Windows 错误码。'}
        }
        $f.Add((New-Finding ('device-errno-'+$code) 'observed' ('设备错误码 '+$code+'：'+$reason) '请一并运行 SD 模式检查空间、属性和关键目录。'))
    }
    if($all -match 'sdprobe.*transaction|sdmmc.*(error|fail|timeout)|SD mount: (?!ESP_OK)'){$f.Add((New-Finding 'device-sd-transport' 'observed' '采集到设备 SD 挂载或传输错误。' '结合原始错误行区分超时、CRC、未就绪等；备份后检查卡和连接。'))}
    if($all -match '时间不可信|时间倒退|请先校准|日期仍未校准'){$f.Add((New-Finding 'clock-protection' 'observed' '设备当前还有时间保护错误；它与 SD 写入失败不是同一个问题。' '使用设备日期设置或保留进度校时工具，诊断器不自动改时间。'))}
    if($all -match 'SD.*无法|SD.*失败|Cannot create|Write/sync failed|writable=0' -and !($all -match '错误码\s*\d+')){$f.Add((New-Finding 'missing-low-level-error' 'limited' '确认有存储失败提示，但本轮没有拿到具体底层错误码。' '保持报错页再采集；旧固件可用 fix55h 后采集，或先运行 SD 模式。'))}
    if($Before -and $After -and ($Before -cne $After)){$f.Add((New-Finding 'status-changed' 'observed' '采集期间设备学习状态发生变化。' '工具只发送 status/web status；检查是否有人操作或设备后台切换，不进行快照恢复。'))}
    if(!$f.Count){$f.Add((New-Finding 'device-no-error-captured' 'limited' '本轮未捕获明确 SD 失败。正常快照不排除间歇性故障。' '在设备出现问题时保持该页面，重新运行设备模式。'))}
    return @($f.ToArray())
}
