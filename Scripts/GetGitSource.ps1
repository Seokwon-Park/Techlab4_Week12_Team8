# Called by SrcSrv. Install beside GitSourceTools.ps1 on the debugging PC.
$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'GitSourceTools.ps1')
try {
    $values = $env:HITORI_GIT_DATA.Split('|')
    $key = $values[1]
    $remote = $values[2]
    $commit = $values[3]
    $path = $values[4]
    $mode = $values[5]
    $hash = $values[6]
    $target = $values[7].Trim('"')
    $cache = Join-Path $env:LOCALAPPDATA ('HitoriDebug\Git\' + $key)
    [IO.Directory]::CreateDirectory($cache) | Out-Null
    if (-not (Test-Path -LiteralPath (Join-Path $cache 'HEAD'))) {
        $null = Invoke-SourceGit $cache @('init', '--bare', '.')
    }
    try { $null = Invoke-SourceGit $cache @('cat-file', '-e', "$commit^{commit}") }
    catch { $null = Invoke-SourceGit $cache @('fetch', '--no-tags', '--depth=1', '--', $remote, $commit) }
    $bytes = Invoke-SourceGit $cache @('cat-file', 'blob', ($commit + ':' + $path))
    $bytes = Convert-SourceEol $bytes $mode
    if ((Get-SourceDigest $bytes) -ne $hash) { throw 'Retrieved Git source checksum differs from build source.' }
    [IO.Directory]::CreateDirectory([IO.Path]::GetDirectoryName($target)) | Out-Null
    $staging = $target + '.' + [Guid]::NewGuid().ToString('N') + '.tmp'
    try {
        [IO.File]::WriteAllBytes($staging, $bytes)
        [IO.File]::Copy($staging, $target, $true)
    } finally { if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging } }
} catch { Write-Error $_ -ErrorAction Continue; exit 1 }
