param([Parameter(Mandatory=$true)][ValidatePattern('^[D-Zd-z]:\\?$')][string]$Drive,[Parameter(Mandatory=$true)][string]$Output,[switch]$ReadOnly)
$ErrorActionPreference='Stop'
. (Join-Path $PSScriptRoot 'diagnostic-core.ps1')
$root=[IO.Path]::GetFullPath($Drive.TrimEnd('\')+'\')
if($root.TrimEnd('\') -ieq $env:SystemDrive){throw 'System drive refused'}
if([IO.Path]::GetFullPath($Output).StartsWith($root,[StringComparison]::OrdinalIgnoreCase)){throw 'Report must not be saved on the diagnosed drive'}
try{
    $info=[IO.DriveInfo]::new($root)
    if(!$info.IsReady){throw 'Drive not ready'}
    $volume=[ordered]@{fileSystem=$info.DriveFormat;freeBytes=$info.AvailableFreeSpace;totalBytes=$info.TotalSize;diskReadOnly=$null;diskStatus=$null}
    try{$disk=Get-Partition -DriveLetter $root[0] -ErrorAction Stop|Get-Disk -ErrorAction Stop;$volume.diskReadOnly=$disk.IsReadOnly;$volume.diskStatus=[string]$disk.OperationalStatus}catch{}
    $r=Invoke-StorageDiagnosis $root -ReadOnly:$ReadOnly
    $r|Add-Member -NotePropertyName volume -NotePropertyValue ([pscustomobject]$volume)
    $r|Add-Member -NotePropertyName findings -NotePropertyValue @(Get-StorageFindings $r)
    Write-DiagJson $Output $r
}catch{Write-DiagJson $Output ([ordered]@{mode='sd';incomplete=$true;message=$_.Exception.Message;findings=@((New-Finding 'sd-diagnosis-incomplete' 'limited' $_.Exception.Message '未完成测试，不算通过；未格式化或修改已有文件。'))});exit 2}
