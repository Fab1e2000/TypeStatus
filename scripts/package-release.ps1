param(
    [Parameter(Mandatory = $true)]
    [string]$ExecutablePath,

    [Parameter(Mandatory = $true)]
    [string]$OutputDirectory,

    [switch]$RequireSignature
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

$repositoryRoot = Split-Path -Parent $PSScriptRoot
$ExecutablePath = [IO.Path]::GetFullPath($ExecutablePath)
$OutputDirectory = [IO.Path]::GetFullPath($OutputDirectory)
if (-not (Test-Path -LiteralPath $ExecutablePath -PathType Leaf)) {
    throw "TypeStatus executable not found: $ExecutablePath"
}
if (Test-Path -LiteralPath $OutputDirectory) {
    throw "Output directory already exists: $OutputDirectory"
}

$signature = Get-AuthenticodeSignature -LiteralPath $ExecutablePath
if ($RequireSignature -and $signature.Status -ne 'Valid') {
    throw "A valid Authenticode signature is required; current status: $($signature.Status)"
}

$version = (Get-Item -LiteralPath $ExecutablePath).VersionInfo.ProductVersion
if ([string]::IsNullOrWhiteSpace($version)) {
    throw 'The executable does not contain a product version.'
}

$stageDirectory = Join-Path $OutputDirectory 'TypeStatus'
New-Item -ItemType Directory -Path $stageDirectory -Force | Out-Null
Copy-Item -LiteralPath $ExecutablePath -Destination (Join-Path $stageDirectory 'TypeStatus.exe')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'LICENSE') -Destination (Join-Path $stageDirectory 'LICENSE.txt')
Copy-Item -LiteralPath (Join-Path $repositoryRoot 'CHANGELOG.md') -Destination $stageDirectory
$guides = @(Get-ChildItem -LiteralPath (Join-Path $repositoryRoot 'docs') -Filter '*.md' -File)
if ($guides.Count -ne 1) {
    throw "Expected exactly one Markdown user guide; found $($guides.Count)."
}
Copy-Item -LiteralPath $guides[0].FullName -Destination (Join-Path $stageDirectory $guides[0].Name)

$zipPath = Join-Path $OutputDirectory 'TypeStatus-windows-x64.zip'
Compress-Archive -Path (Join-Path $stageDirectory '*') -DestinationPath $zipPath
$hash = Get-FileHash -LiteralPath $zipPath -Algorithm SHA256
$checksumPath = Join-Path $OutputDirectory 'TypeStatus-windows-x64.zip.sha256'
Set-Content -LiteralPath $checksumPath -Encoding ascii -NoNewline -Value (
    $hash.Hash.ToLowerInvariant() + ' *TypeStatus-windows-x64.zip'
)

[pscustomobject]@{
    Version = $version
    SignatureStatus = $signature.Status
    ZipPath = $zipPath
    ZipBytes = (Get-Item -LiteralPath $zipPath).Length
    Sha256 = $hash.Hash.ToLowerInvariant()
    ChecksumPath = $checksumPath
} | Format-List
