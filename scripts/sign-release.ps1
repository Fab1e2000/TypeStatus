param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$CertificatePath,

    [string]$PasswordEnvironmentVariable = 'TYPESTATUS_SIGNING_PASSWORD',

    [string]$TimestampServer = 'http://timestamp.digicert.com'
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$ExecutablePath = [IO.Path]::GetFullPath($ExecutablePath)
$CertificatePath = [IO.Path]::GetFullPath($CertificatePath)
$password = [Environment]::GetEnvironmentVariable($PasswordEnvironmentVariable)
if ([string]::IsNullOrEmpty($password)) {
    throw "Signing password environment variable is empty: $PasswordEnvironmentVariable"
}

$signTool = Get-Command signtool.exe -ErrorAction SilentlyContinue
if ($null -eq $signTool) {
    $kitsRoot = Join-Path ${env:ProgramFiles(x86)} 'Windows Kits\10\bin'
    $signTool = Get-ChildItem -LiteralPath $kitsRoot -Filter signtool.exe -Recurse -ErrorAction SilentlyContinue |
        Where-Object { $_.FullName -match '\\x64\\signtool\.exe$' } |
        Sort-Object FullName -Descending |
        Select-Object -First 1
}
if ($null -eq $signTool) {
    throw 'signtool.exe was not found. Install the Windows SDK signing tools.'
}
$signToolPath = if ($signTool -is [System.Management.Automation.CommandInfo]) {
    $signTool.Source
} else {
    $signTool.FullName
}

& $signToolPath sign /fd SHA256 /td SHA256 /tr $TimestampServer `
    /f $CertificatePath /p $password $ExecutablePath
if ($LASTEXITCODE -ne 0) {
    throw "signtool sign failed with exit code $LASTEXITCODE"
}
& $signToolPath verify /pa /v $ExecutablePath
if ($LASTEXITCODE -ne 0) {
    throw "signtool verify failed with exit code $LASTEXITCODE"
}
