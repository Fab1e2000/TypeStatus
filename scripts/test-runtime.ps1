param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExecutablePath = [IO.Path]::GetFullPath($ExecutablePath)
if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) {
    throw "TypeStatus executable not found: $ExecutablePath"
}
if (@(Get-Process TypeStatus -ErrorAction SilentlyContinue).Count -ne 0) {
    throw 'Close all running TypeStatus instances before running the smoke test.'
}

Add-Type -TypeDefinition @'
using System;
using System.Text;
using System.Runtime.InteropServices;
public static class TypeStatusSmokeWindow {
    public delegate bool EnumWindowsProc(IntPtr hwnd, IntPtr state);
    [DllImport("user32.dll")] public static extern bool EnumWindows(EnumWindowsProc callback, IntPtr state);
    [DllImport("user32.dll", CharSet = CharSet.Unicode)] public static extern int GetClassName(IntPtr hwnd, StringBuilder name, int length);
    [DllImport("user32.dll")] public static extern bool PostMessage(IntPtr hwnd, uint message, IntPtr wParam, IntPtr lParam);
    [DllImport("user32.dll")] public static extern bool SystemParametersInfo(uint action, uint parameter, IntPtr value, uint flags);
    public static IntPtr Find() {
        IntPtr result = IntPtr.Zero;
        EnumWindows((hwnd, state) => {
            var name = new StringBuilder(256);
            GetClassName(hwnd, name, name.Capacity);
            if (name.ToString() == "TypeStatus.HiddenWindow") {
                result = hwnd;
                return false;
            }
            return true;
        }, IntPtr.Zero);
        return result;
    }
}
'@

$runKeyPath = 'Software\Microsoft\Windows\CurrentVersion\Run'
$runValueName = 'TypeStatus'
$runKey = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($runKeyPath, $true)
if ($null -eq $runKey) {
    $runKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($runKeyPath)
}
$originalExists = $runKey.GetValueNames() -contains $runValueName
$originalValue = if ($originalExists) {
    $runKey.GetValue($runValueName, $null, [Microsoft.Win32.RegistryValueOptions]::DoNotExpandEnvironmentNames)
} else { $null }
$originalKind = if ($originalExists) { $runKey.GetValueKind($runValueName) } else { $null }
$runKey.DeleteValue($runValueName, $false)
$runKey.Close()

$mainProcess = $null
$watchdogProcess = $null
$normalProcess = $null
$normalWatchdog = $null
try {
    $mainProcess = Start-Process -FilePath $ExecutablePath -PassThru
    Start-Sleep -Milliseconds 800
    if ($mainProcess.HasExited) {
        throw "TypeStatus exited during startup with code $($mainProcess.ExitCode)."
    }

    $window = [TypeStatusSmokeWindow]::Find()
    if ($window -eq [IntPtr]::Zero) {
        throw 'TypeStatus hidden window was not found.'
    }

    $watchdogInfo = Get-CimInstance Win32_Process |
        Where-Object {
            $_.Name -eq 'TypeStatus.exe' -and
            $_.CommandLine -match ('--watchdog\s+' + $mainProcess.Id)
        } |
        Select-Object -First 1
    if ($null -eq $watchdogInfo) {
        throw 'The cursor recovery watchdog was not started.'
    }
    $watchdogProcess = Get-Process -Id $watchdogInfo.ProcessId -ErrorAction Stop

    [TypeStatusSmokeWindow]::PostMessage($window, 0x0111, [IntPtr]1002, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 250
    $configured = (Get-ItemProperty -LiteralPath 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name TypeStatus -ErrorAction Stop).TypeStatus
    $expected = '"' + $ExecutablePath + '"'
    if (-not [string]::Equals($configured, $expected, [StringComparison]::OrdinalIgnoreCase)) {
        throw "Unexpected startup command: $configured"
    }

    [TypeStatusSmokeWindow]::PostMessage($window, 0x0111, [IntPtr]1002, [IntPtr]::Zero) | Out-Null
    Start-Sleep -Milliseconds 250
    $remainingItem = Get-ItemProperty -LiteralPath 'HKCU:\Software\Microsoft\Windows\CurrentVersion\Run' -Name TypeStatus -ErrorAction SilentlyContinue
    if ($null -ne $remainingItem) {
        throw 'The startup registry value was not removed.'
    }

    Stop-Process -Id $mainProcess.Id -Force
    $mainProcess.WaitForExit(5000) | Out-Null
    $watchdogProcess.WaitForExit(5000) | Out-Null
    if (-not $watchdogProcess.HasExited) {
        throw 'The watchdog did not exit after the main process ended.'
    }
    $recoveryProbe = Start-Process -FilePath $ExecutablePath `
        -ArgumentList '--watchdog', '4294967295' -Wait -PassThru
    if ($recoveryProbe.ExitCode -ne 0) {
        throw 'The watchdog cursor recovery API returned an error.'
    }

    $normalProcess = Start-Process -FilePath $ExecutablePath -PassThru
    Start-Sleep -Milliseconds 800
    $normalWindow = [TypeStatusSmokeWindow]::Find()
    $normalWatchdogInfo = Get-CimInstance Win32_Process |
        Where-Object {
            $_.Name -eq 'TypeStatus.exe' -and
            $_.CommandLine -match ('--watchdog\s+' + $normalProcess.Id)
        } |
        Select-Object -First 1
    if ($normalWindow -eq [IntPtr]::Zero -or $null -eq $normalWatchdogInfo) {
        throw 'The normal-exit test instance did not initialize.'
    }
    $normalWatchdog = Get-Process -Id $normalWatchdogInfo.ProcessId -ErrorAction Stop
    [TypeStatusSmokeWindow]::PostMessage($normalWindow, 0x0010, [IntPtr]::Zero, [IntPtr]::Zero) | Out-Null
    $normalProcess.WaitForExit(5000) | Out-Null
    $normalWatchdog.WaitForExit(5000) | Out-Null
    if (-not $normalProcess.HasExited -or -not $normalWatchdog.HasExited) {
        throw 'The main process or watchdog remained after a normal exit.'
    }

    Write-Host 'Runtime smoke test passed:' -ForegroundColor Green
    Write-Host '  startup registry toggle: passed'
    Write-Host '  watchdog launch: passed'
    Write-Host '  forced-exit cursor recovery: passed'
    Write-Host '  normal-exit watchdog handoff: passed'
}
finally {
    if ($null -ne $mainProcess -and -not $mainProcess.HasExited) {
        Stop-Process -Id $mainProcess.Id -Force -ErrorAction SilentlyContinue
    }
    if ($null -ne $watchdogProcess -and -not $watchdogProcess.HasExited) {
        Stop-Process -Id $watchdogProcess.Id -Force -ErrorAction SilentlyContinue
    }
    if ($null -ne $normalProcess -and -not $normalProcess.HasExited) {
        Stop-Process -Id $normalProcess.Id -Force -ErrorAction SilentlyContinue
    }
    if ($null -ne $normalWatchdog -and -not $normalWatchdog.HasExited) {
        Stop-Process -Id $normalWatchdog.Id -Force -ErrorAction SilentlyContinue
    }
    [TypeStatusSmokeWindow]::SystemParametersInfo(0x0057, 0, [IntPtr]::Zero, 0) | Out-Null

    $restoreKey = [Microsoft.Win32.Registry]::CurrentUser.OpenSubKey($runKeyPath, $true)
    if ($null -eq $restoreKey) {
        $restoreKey = [Microsoft.Win32.Registry]::CurrentUser.CreateSubKey($runKeyPath)
    }
    if ($originalExists) {
        $restoreKey.SetValue($runValueName, $originalValue, $originalKind)
    } else {
        $restoreKey.DeleteValue($runValueName, $false)
    }
    $restoreKey.Close()
}
