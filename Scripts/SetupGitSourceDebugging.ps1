# Run once on each debugging PC, then restart Visual Studio/WinDbg.
[CmdletBinding()]
param([switch]$SessionOnly)
$ErrorActionPreference = 'Stop'
Get-Command git.exe -ErrorAction Stop | Out-Null
$directory = Join-Path $env:LOCALAPPDATA 'HitoriDebug\SourceTools'
[IO.Directory]::CreateDirectory($directory) | Out-Null
foreach ($name in @('GetGitSource.ps1', 'GitSourceTools.ps1')) {
    Copy-Item -LiteralPath (Join-Path $PSScriptRoot $name) -Destination (Join-Path $directory $name) -Force
}
$command = 'cmd.exe /c powershell.exe -NoProfile -NonInteractive -ExecutionPolicy Bypass -File "' +
    (Join-Path $directory 'GetGitSource.ps1') + '"'
$env:HITORI_GIT_SOURCE_COMMAND = $command
if (-not $SessionOnly) {
    [Environment]::SetEnvironmentVariable('HITORI_GIT_SOURCE_COMMAND', $command, 'User')
}
Write-Host "Git source retrieval configured: $directory"
if (-not $SessionOnly) { Write-Host 'Restart the debugger to pick up the user environment variable.' }
