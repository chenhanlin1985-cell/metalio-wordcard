param([ValidateSet('Menu','Device','SD')][string]$Mode='Menu',[string]$Port='', [string]$Drive='', [string]$OutputDirectory='', [switch]$ReadOnly,[switch]$NoPrompt,[switch]$SkipFilesystemScan)
$ErrorActionPreference='Stop'
[Console]::InputEncoding=[Text.UTF8Encoding]::new($false)
[Console]::OutputEncoding=[Text.UTF8Encoding]::new($false);$OutputEncoding=[Console]::OutputEncoding
. (Join-Path $PSScriptRoot 'diagnostic-core.ps1')
if(!$OutputDirectory){$OutputDirectory=Join-Path $env:LOCALAPPDATA ('WordcardDiagnostics\'+[DateTime]::Now.ToString('yyyyMMdd-HHmmss')+'-'+[Guid]::NewGuid().ToString('N').Substring(0,6))}
$OutputDirectory=[IO.Path]::GetFullPath($OutputDirectory)
if(Test-Path -LiteralPath $OutputDirectory){throw 'Output directory already exists. Choose a new directory to preserve the prior report.'}
$null=New-Item -ItemType Directory -Path $OutputDirectory
$report=[ordered]@{tool='wordcard-sd-diagnostics-1.0.1';created=[DateTimeOffset]::Now.ToString('o');firmwareFlashed=$false;clockChanged=$false;learningActionsSent=0;automaticRepairs=$false;findings=@()}
function Protect-SerialLines([string[]]$Lines) {
    foreach($line in $Lines){
        if($line -match 'WEB_STATUS (.*)$'){
            try{$s=$Matches[1]|ConvertFrom-Json;'WEB_STATUS '+(@{engine=$s.engine;courseError=$s.courseError;profileError=$s.profileError;stateFingerprint=[WordcardDiagIO]::HashText(($s|ConvertTo-Json -Depth 20 -Compress))}|ConvertTo-Json -Compress)}catch{'WEB_STATUS invalid-or-truncated-json'}
        }elseif($line -match 'DICT_FLOW .*writable=(\d+)'){'DICT_FLOW writable='+$Matches[1]}
        elseif($line -match 'sdprobe.*transaction|sdmmc|SD mount:|STORAGE_CHECK|State protected|Progress protected|wordcard: (SD|RTC|时间：|WEB_PROFILE|WEB_AUDIO|AUDIO_STATE|PERF)|Cannot create|Write/sync failed|Backup is not writable|Cannot back up|Cannot commit|无法恢复|错误码'){$line}
    }
}
function Read-StatusObject([string[]]$Lines){
    $entry=$Lines|Where-Object{$_ -match 'WEB_STATUS '}|Select-Object -Last 1
    if($entry){try{return (($entry -split 'WEB_STATUS ',2)[1]|ConvertFrom-Json)}catch{}}
    return $null
}
function Send-DiagnosticCommand([IO.Ports.SerialPort]$Serial,[string]$Command,[Collections.Generic.List[string]]$Capture){
    if($Command -notin @('web status','status')){throw 'Command is not on the read-only allowlist'}
    $Serial.WriteLine($Command);$end=[DateTime]::UtcNow.AddSeconds(12);$lines=[Collections.Generic.List[string]]::new()
    while([DateTime]::UtcNow -lt $end){
        try{$s=$Serial.ReadLine().TrimEnd("`r")}catch [TimeoutException]{continue}
        $lines.Add($s);$Capture.Add($s)
        if($s.EndsWith('Command: '+$Command)){return @($lines.ToArray())}
    }
    throw ('设备未在12秒内完整应答；未自动重发：'+$Command)
}
try {
    Write-Host '单词机 SD 诊断 · 保留进度和词表' -ForegroundColor Cyan
    Write-Host '不刷机、不校时、不格式化、不删除已有文件。报告不收集单词表和题目内容。'
    if($Mode -eq 'Menu'){
        Write-Host '1. 设备模式：普通学习界面接 USB，读取当前报错（先选这个）'
        Write-Host '2. SD 模式：已进入 U 盘模式，或用读卡器连接 SD'
        $choice=Read-Host '输入 1 或 2，直接回车选 1'
        $Mode=if($choice -eq '2'){'SD'}elseif(!$choice -or $choice -eq '1'){'Device'}else{throw '请选择1或2'}
    }
    $report.mode=$Mode
    if($Mode -eq 'Device'){
        $ports=@([IO.Ports.SerialPort]::GetPortNames()|Sort-Object)
        if(!$ports.Count){throw '电脑没有串口。普通学习模式请检查数据线；如果当前是 U 盘模式，请改选 SD 模式。'}
        if(!$Port){
            $esp=@();try{$esp=@(Get-CimInstance Win32_SerialPort|Where-Object{$_.PNPDeviceID -match 'VID_303A&PID_1001'}|ForEach-Object{$_.DeviceID})}catch{}
            if($esp.Count -eq 1){$Port=$esp[0]}elseif($ports.Count -eq 1){$Port=$ports[0]}
            elseif($NoPrompt){throw '多个串口，请显式指定 -Port COMxx'}else{Write-Host ('可用串口：'+($ports -join ', '));$Port=Read-Host '输入单词机串口，例如 COM10'}
        }
        if($Port -notmatch '^COM\d+$' -or $Port -notin $ports){throw '串口不在当前可用列表中'}
        if(!$NoPrompt){Write-Host '请在设备上停留于“无法写入/进度问题”的报错页。采集期间不要操作。';$null=Read-Host '准备好后按回车；没有报错也可采集当前状态'}
        $serial=[IO.Ports.SerialPort]::new($Port,115200,[IO.Ports.Parity]::None,8,[IO.Ports.StopBits]::One)
        $serial.DtrEnable=$false;$serial.RtsEnable=$false;$serial.ReadTimeout=500;$serial.WriteTimeout=2000;$serial.Encoding=[Text.UTF8Encoding]::new($false,$true)
        $capture=[Collections.Generic.List[string]]::new();$before=$null;$after=$null
        try {
            $serial.Open();Start-Sleep -Milliseconds 200
            $initial=$serial.ReadExisting();foreach($l in $initial.Split("`n")){$capture.Add($l.TrimEnd("`r"))}
            $serial.WriteLine('');$a=Send-DiagnosticCommand $serial 'web status' $capture;$before=Read-StatusObject $a
            if(!$before -or $before.engine -ne 'native-vocabulary-fsrs6'){throw '未识别到本项目原生背词引擎。未发送任何学习或修复操作。'}
            $null=Send-DiagnosticCommand $serial 'status' $capture
            $b=Send-DiagnosticCommand $serial 'web status' $capture;$after=Read-StatusObject $b
            if(!$after){throw '第二次状态缺失，无法完成前后对照'}
            $beforeHash=[WordcardDiagIO]::HashText(($before|ConvertTo-Json -Depth 20 -Compress));$afterHash=[WordcardDiagIO]::HashText(($after|ConvertTo-Json -Depth 20 -Compress))
            $report.device=@{recognized=$true;stateUnchanged=($beforeHash -ceq $afterHash);beforeFingerprint=$beforeHash;afterFingerprint=$afterHash;courseError=$after.courseError;profileError=$after.profileError;sentCommands=@('web status','status','web status')}
            $report.findings=@(Get-DeviceFindings @($capture.ToArray()) $beforeHash $afterHash)
        }finally{
            if($serial.IsOpen){$serial.Close()};$serial.Dispose()
            [IO.File]::WriteAllLines((Join-Path $OutputDirectory '设备日志-已去题目内容.txt'),[string[]]@(Protect-SerialLines @($capture.ToArray())),[Text.UTF8Encoding]::new($false))
        }
    } else {
        if(!$Drive){
            Write-Host '请确认设备已显示 U 盘就绪，或 SD 已接入读卡器。无法进入 U 盘时不要强制切换；可先发设备报告。'
            $candidates=@([IO.DriveInfo]::GetDrives()|Where-Object{$_.IsReady -and $_.Name.TrimEnd('\') -ine $env:SystemDrive -and (Test-Path -LiteralPath (Join-Path $_.RootDirectory.FullName 'metalio/e-ink/wordcards') -PathType Container)})
            foreach($c in $candidates){Write-Host ($c.Name+'  '+$c.DriveFormat+'  '+[math]::Round($c.AvailableFreeSpace/1GB,2)+' GB 可用')}
            if($NoPrompt){throw 'SD 模式请明确指定 -Drive X:'}
            $Drive=Read-Host '输入单词机 SD 盘符（例如 G:），不要选电脑磁盘'
        }
        if($Drive -notmatch '^[D-Zd-z]:\\?$'){throw '只接受 SD 盘符，例如 G:'}
        $Drive=$Drive.Substring(0,2).ToUpperInvariant()
        if($Drive -ieq $env:SystemDrive){throw '不能诊断系统盘'}
        if($OutputDirectory.StartsWith($Drive+'\',[StringComparison]::OrdinalIgnoreCase)){throw '报告目录不能在被诊断的 SD 卡上'}
        if(!$ReadOnly){Write-Host '将用随机临时文件逐项检查创建、写入、落盘、追加、回读、改名、删除；已有文件只读。'}
        $worker=Join-Path $PSScriptRoot 'sd-worker.ps1';$result=Join-Path $OutputDirectory 'sd-result.json'
        foreach($p in @($worker,$result)){if($p.Contains('"')){throw 'Unsupported quote in path'}}
        $args='-NoProfile -ExecutionPolicy Bypass -File "'+$worker+'" -Drive '+$Drive+' -Output "'+$result+'"'
        if($ReadOnly){$args+=' -ReadOnly'}
        $proc=Start-Process -FilePath (Join-Path $env:SystemRoot 'System32/WindowsPowerShell/v1.0/powershell.exe') -ArgumentList $args -WindowStyle Hidden -PassThru -RedirectStandardOutput (Join-Path $OutputDirectory 'worker-output.txt') -RedirectStandardError (Join-Path $OutputDirectory 'worker-error.txt')
        if(!$proc.WaitForExit(90000)){
            try{$proc.Kill()}catch{}
            throw 'SD 检查超过90秒，已停止诊断进程；不算通过。仅随机测试文件可能遗留。不要强拔卡，请待 Windows 可安全弹出后再移除。'
        }
        if(!(Test-Path -LiteralPath $result)){throw 'SD 检查未产生完整报告，请保留 worker-error.txt'}
        $report.sd=[IO.File]::ReadAllText($result)|ConvertFrom-Json;$report.findings=@($report.sd.findings)
        # Read-only chkdsk: no /f, /r, /x, /scan or automatic repair switches.
        # Separate from quick tests; report permission failure/timeout without escalation.
        $report.chkdsk=@{attempted=$false;repairsRequested=$false}
        if(!$report.sd.incomplete -and !$SkipFilesystemScan){
            $report.chkdsk.attempted=$true
            $psi=[Diagnostics.ProcessStartInfo]::new();$psi.FileName=Join-Path $env:SystemRoot 'System32/chkdsk.exe';$psi.Arguments=$Drive
            $psi.UseShellExecute=$false;$psi.CreateNoWindow=$true;$psi.RedirectStandardOutput=$true;$psi.RedirectStandardError=$true
            $psi.StandardOutputEncoding=[WordcardDiagIO]::NativeDiskEncoding();$psi.StandardErrorEncoding=$psi.StandardOutputEncoding
            $report.chkdsk.outputCodePage=$psi.StandardOutputEncoding.CodePage
            $p=[Diagnostics.Process]::new();$p.StartInfo=$psi;$null=$p.Start()
            $outTask=$p.StandardOutput.ReadToEndAsync();$errTask=$p.StandardError.ReadToEndAsync()
            if($p.WaitForExit(45000)){
                $out=$outTask.Result+"`n"+$errTask.Result
                # Hide volume labels and serial identifiers, which are unrelated to this fault.
                $out=[regex]::Replace($out,'(?im)^.*(卷标|序列号|volume label|volume serial).*$','[volume identity omitted]')
                [IO.File]::WriteAllText((Join-Path $OutputDirectory '文件系统只读检查.txt'),$out,[Text.UTF8Encoding]::new($false))
                $report.chkdsk.exitCode=$p.ExitCode;$report.chkdsk.completed=$true
                if($p.ExitCode -ne 0){$report.findings+=,(New-Finding 'chkdsk-needs-review' 'limited' ('只读文件系统检查退出码 '+$p.ExitCode+'，可能是检查发现问题或权限不足。') '查看文件系统只读检查.txt；工具不自动修复。')}
            }else{try{$p.Kill()}catch{};$report.chkdsk.completed=$false;$report.findings+=,(New-Finding 'chkdsk-timeout' 'limited' '只读文件系统检查超过45秒，结果未完成。' '未执行修复；先保留报告和原数据。')}
            $p.Dispose()
        }
    }
}catch{
    $report.incomplete=$true;$report.findings+=,(New-Finding 'diagnosis-incomplete' 'limited' $_.Exception.Message '本次未完成的项目不算通过。保留报告，按提示重新连接；不要清空记录。')
}finally{
    Write-DiagJson (Join-Path $OutputDirectory 'diagnosis.json') $report
    $summary=[Collections.Generic.List[string]]::new();$summary.Add('单词机 SD 诊断结果');$summary.Add('')
    foreach($f in $report.findings){$summary.Add('['+$f.certainty+'] '+$f.message);$summary.Add('下一步：'+$f.next);$summary.Add('')}
    if($report.device){$summary.Add('设备前后学习状态一致：'+$report.device.stateUnchanged)}
    if($report.sd){$summary.Add('已验证原文件数：'+$report.sd.filesVerified+'；无法完整验证数：'+$report.sd.unverifiedFiles);$proof=if($null -eq $report.sd.progressUnchanged){'未能完整验证'}else{[string]$report.sd.progressUnchanged};$summary.Add('已有记录/词表前后哈希及属性保持：'+$proof)}
    $summary.Add('本工具未刷机、校时、格式化、修复文件系统、清空进度、重发答案或导入。');$summary.Add('小文件测试不能证明整张卡或设备 SDMMC 通路完全健康。')
    [IO.File]::WriteAllLines((Join-Path $OutputDirectory '先看诊断结果.txt'),$summary.ToArray(),[Text.UTF8Encoding]::new($false))
    foreach($s in $summary){Write-Host $s}
    Write-Host ('报告位置：'+$OutputDirectory) -ForegroundColor Cyan
    try{Compress-Archive -LiteralPath @((Get-ChildItem -LiteralPath $OutputDirectory -File).FullName) -DestinationPath ($OutputDirectory+'.zip') -CompressionLevel Optimal -ErrorAction Stop;Write-Host ('请发回这个报告包：'+$OutputDirectory+'.zip') -ForegroundColor Cyan}catch{Write-Host '报告压缩未完成，把上面的报告文件夹手动压缩发回即可。'}
    Write-Host '不需要发送词表或学习记录。可双击“查看诊断报告.cmd”打开默认报告目录。'
}
