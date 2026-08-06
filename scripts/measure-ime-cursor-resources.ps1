param(
    [ValidateRange(5, 3600)]
    [int]$DurationSeconds = 30,

    [ValidateRange(200, 10000)]
    [int]$IntervalMs = 1000,

    [int]$TargetProcessId = 0,

    [string]$LogPath = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

Add-Type -TypeDefinition @'
using System;
using System.Runtime.InteropServices;
public static class TypeStatusGuiResources {
    [DllImport("user32.dll")]
    public static extern uint GetGuiResources(IntPtr process, uint flags);
}
'@

function Find-ImeCursorProcess {
    $candidates = @(
        Get-CimInstance Win32_Process |
            Where-Object {
                $_.ProcessId -ne $PID -and
                (
                    (
                        $_.Name -eq 'TypeStatus.exe' -and
                        $_.CommandLine -notmatch '--watchdog'
                    ) -or
                    (
                        ($_.Name -eq 'powershell.exe' -or $_.Name -eq 'pwsh.exe') -and
                        $_.CommandLine -and
                        $_.CommandLine -match 'ime-cursor-status\.ps1'
                    )
                )
            }
    )

    if ($candidates.Count -eq 0) {
        throw 'No running TypeStatus.exe or ime-cursor-status.ps1 process was found.'
    }

    if ($candidates.Count -gt 1) {
        Write-Host 'More than one matching process was found:' -ForegroundColor Yellow
        $candidates |
            Select-Object ProcessId, Name, CommandLine |
            Format-Table -AutoSize
        throw 'Run again with -TargetProcessId <PID>.'
    }

    return [int]$candidates[0].ProcessId
}

if ($TargetProcessId -eq 0) {
    $TargetProcessId = Find-ImeCursorProcess
}

$target = Get-Process -Id $TargetProcessId -ErrorAction Stop
$logicalProcessorCount = [Environment]::ProcessorCount

if ([string]::IsNullOrWhiteSpace($LogPath)) {
    $fileName = 'ime-cursor-resources-{0:yyyyMMdd-HHmmss}.csv' -f (Get-Date)
    $LogPath = Join-Path (Get-Location) $fileName
}
else {
    $LogPath = [IO.Path]::GetFullPath($LogPath)
}

Write-Host ('Monitoring PID {0} ({1}) for {2} seconds...' -f
    $target.Id,
    $target.ProcessName,
    $DurationSeconds) -ForegroundColor Cyan
Write-Host ('Logical processors: {0}; interval: {1} ms' -f
    $logicalProcessorCount,
    $IntervalMs)

$samples = [Collections.Generic.List[object]]::new()
$previousCpuSeconds = $target.TotalProcessorTime.TotalSeconds
$previousTimestamp = [Diagnostics.Stopwatch]::GetTimestamp()
$stopAt = [DateTime]::UtcNow.AddSeconds($DurationSeconds)

while ([DateTime]::UtcNow -lt $stopAt) {
    Start-Sleep -Milliseconds $IntervalMs

    try {
        $target.Refresh()
        if ($target.HasExited) {
            Write-Warning 'The monitored process exited before sampling completed.'
            break
        }

        $nowTimestamp = [Diagnostics.Stopwatch]::GetTimestamp()
        $currentCpuSeconds = $target.TotalProcessorTime.TotalSeconds
        $elapsedSeconds = ($nowTimestamp - $previousTimestamp) /
            [double][Diagnostics.Stopwatch]::Frequency
        $cpuDeltaSeconds = $currentCpuSeconds - $previousCpuSeconds

        # Normalize by logical processor count to match Task Manager-style CPU %.
        $cpuPercent = if ($elapsedSeconds -gt 0) {
            100.0 * $cpuDeltaSeconds / $elapsedSeconds / $logicalProcessorCount
        }
        else {
            0.0
        }

        $sample = [pscustomobject]@{
            Timestamp          = (Get-Date).ToString('o')
            ProcessId          = $target.Id
            CpuPercent         = [Math]::Round($cpuPercent, 4)
            WorkingSetBytes    = $target.WorkingSet64
            PrivateMemoryBytes = $target.PrivateMemorySize64
            HandleCount        = $target.HandleCount
            GdiObjectCount     = [TypeStatusGuiResources]::GetGuiResources($target.Handle, 0)
            UserObjectCount    = [TypeStatusGuiResources]::GetGuiResources($target.Handle, 1)
            ThreadCount        = $target.Threads.Count
        }

        $samples.Add($sample)
        Write-Host ('{0:HH:mm:ss}  CPU={1,7:N4}%  WS={2,7:N2} MB  Private={3,7:N2} MB  Handles={4}  GDI={5}  USER={6}  Threads={7}' -f
            (Get-Date),
            $sample.CpuPercent,
            ($sample.WorkingSetBytes / 1MB),
            ($sample.PrivateMemoryBytes / 1MB),
            $sample.HandleCount,
            $sample.GdiObjectCount,
            $sample.UserObjectCount,
            $sample.ThreadCount)

        $previousCpuSeconds = $currentCpuSeconds
        $previousTimestamp = $nowTimestamp
    }
    catch [System.InvalidOperationException] {
        Write-Warning 'The monitored process exited before sampling completed.'
        break
    }
}

if ($samples.Count -eq 0) {
    throw 'No resource samples were collected.'
}

$samples | Export-Csv -Path $LogPath -NoTypeInformation -Encoding UTF8

$cpuStats = $samples | Measure-Object -Property CpuPercent -Average -Maximum
$workingSetStats = $samples | Measure-Object -Property WorkingSetBytes -Average -Maximum
$privateStats = $samples | Measure-Object -Property PrivateMemoryBytes -Average -Maximum
$handleStats = $samples | Measure-Object -Property HandleCount -Minimum -Maximum
$gdiStats = $samples | Measure-Object -Property GdiObjectCount -Minimum -Maximum
$userStats = $samples | Measure-Object -Property UserObjectCount -Minimum -Maximum

Write-Host ''
Write-Host 'Summary' -ForegroundColor Green
Write-Host ('  CPU:         avg={0:N4}%  max={1:N4}%' -f
    $cpuStats.Average,
    $cpuStats.Maximum)
Write-Host ('  Working set: avg={0:N2} MB  max={1:N2} MB' -f
    ($workingSetStats.Average / 1MB),
    ($workingSetStats.Maximum / 1MB))
Write-Host ('  Private mem: avg={0:N2} MB  max={1:N2} MB' -f
    ($privateStats.Average / 1MB),
    ($privateStats.Maximum / 1MB))
Write-Host ('  Handles:     min={0}  max={1}' -f
    $handleStats.Minimum,
    $handleStats.Maximum)
Write-Host ('  GDI objects: min={0}  max={1}' -f
    $gdiStats.Minimum,
    $gdiStats.Maximum)
Write-Host ('  USER objects: min={0}  max={1}' -f
    $userStats.Minimum,
    $userStats.Maximum)
Write-Host ('  CSV:         {0}' -f ([IO.Path]::GetFullPath($LogPath)))
