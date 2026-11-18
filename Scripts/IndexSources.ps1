<#
Indexes native PDBs against checksum-verified Git commits. No source snapshots are published.
Run after linking and before symstore add. Requires Windows SDK Debugging Tools.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PdbPath,
    [string]$RepositoryRoot,
    [string]$ToolsDirectory = "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\srcsrv"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$temporaryStream = $null
$temporaryPdb = $null
$validated = $false
. (Join-Path $PSScriptRoot 'GitSourceTools.ps1')
function Git-Text([string]$Directory, [string[]]$GitArguments) {
    [Text.Encoding]::UTF8.GetString((Invoke-SourceGit $Directory $GitArguments)).Trim()
}
function Assert-StreamValue([string]$Value) {
    if ($Value -match '[%*|\x00-\x1f]') { throw "Unsupported SrcSrv value: $Value" }
}
try {
    if (-not $RepositoryRoot) { $RepositoryRoot = Split-Path $PSScriptRoot -Parent }
    $pdb = (Resolve-Path -LiteralPath $PdbPath).Path
    $repository = (Resolve-Path -LiteralPath $RepositoryRoot).Path.TrimEnd('\') + '\'
    $repositories = @{}
    $srcTool = Join-Path $ToolsDirectory 'srctool.exe'
    $pdbStr = Join-Path $ToolsDirectory 'pdbstr.exe'
    foreach ($tool in @($srcTool, $pdbStr)) {
        if (-not (Test-Path -LiteralPath $tool -PathType Leaf)) { throw "Missing SDK tool: $tool" }
    }
    $rawSources = @(& $srcTool -r -h -z $pdb)
    if ($LASTEXITCODE -ne 0) { throw "Cannot read source checksums from $pdb" }

    $entries = [Collections.Generic.List[object]]::new()
    $seen = @{}
    $externalCount = 0
    $thirdPartyCount = 0
    $thirdPartyRoot = Join-Path $repository 'Source\ThirdParty\'
    $generatedCount = 0
    $generatedRoot = Join-Path $repository 'Intermediate\Generated\'
    foreach ($line in $rawSources) {
        if ($line -notmatch '^(.*?)\t\s*Checksum (MD5|SHA1|SHA256):\s*([0-9A-Fa-f]+)\s*$') { continue }
        $original = $Matches[1]
        $algorithm = $Matches[2]
        $expected = $Matches[3]
        if (-not $original.StartsWith($repository, [StringComparison]::OrdinalIgnoreCase)) {
            $externalCount++
            continue
        }
        # Third-party sources are not published through our Git source index.
        if ($original.StartsWith($thirdPartyRoot, [StringComparison]::OrdinalIgnoreCase)) {
            $thirdPartyCount++
            continue
        }
        Assert-StreamValue $original
        if ($seen.ContainsKey($original)) {
            if ($seen[$original] -ne "$algorithm`:$expected") { throw "Conflicting PDB checksums: $original. Rebuild required." }
            continue
        }
        $seen[$original] = "$algorithm`:$expected"
        if (-not (Test-Path -LiteralPath $original -PathType Leaf)) { throw "Build source missing: $original. Rebuild required." }
        $bytes = [IO.File]::ReadAllBytes($original)
        $hasher = [Security.Cryptography.HashAlgorithm]::Create($algorithm)
        try { $actual = [BitConverter]::ToString($hasher.ComputeHash($bytes)).Replace('-', '') }
        finally { $hasher.Dispose() }
        if ($actual -ne $expected) { throw "Source differs from PDB: $original. Rebuild required." }
        # HHT output is checked against the PDB, but cannot be retrieved from Git.
        if ($original.StartsWith($generatedRoot, [StringComparison]::OrdinalIgnoreCase)) {
            $generatedCount++
            continue
        }
        # Resolve the owning repository separately for submodule sources.
        $owner = Git-Text ([IO.Path]::GetDirectoryName($original)) @('rev-parse', '--show-toplevel')
        if (-not $repositories.ContainsKey($owner)) {
            $commit = Git-Text $owner @('rev-parse', 'HEAD')
            $remote = Git-Text $owner @('config', '--get', 'remote.origin.url')
            # Never embed credentials or a build-machine-local remote in published PDBs.
            if ($remote -match '[?#]' -or $remote -notmatch '^(https://[^/@]+/[^\s]+|ssh://(?:git@)?[^/]+/[^\s]+|git@[^:]+:[^\s]+)$') {
                throw 'Source indexing requires a credential-free HTTPS or SSH origin URL.'
            }
            Assert-StreamValue $remote
            $key = (Get-SourceDigest ([Text.Encoding]::UTF8.GetBytes($remote))).Substring(0, 16)
            $repositories[$owner] = @{ Commit = $commit; Remote = $remote; Key = $key }
        }
        $info = $repositories[$owner]
        $relative = $original.Substring($owner.Length + 1).Replace('\', '/')
        Assert-StreamValue $relative
        # A generated/untracked file or uncommitted change must never point at unrelated HEAD content.
        $blob = Invoke-SourceGit $owner @('cat-file', 'blob', ($info.Commit + ':' + $relative))
        $mode = 'raw'
        if ((Get-SourceDigest $blob $algorithm) -ne $expected.ToLowerInvariant()) {
            $mode = 'crlf'
            if ((Get-SourceDigest (Convert-SourceEol $blob $mode) $algorithm) -ne $expected.ToLowerInvariant()) {
                throw "Build source differs from Git HEAD: $original. Commit changes and rebuild before publishing."
            }
        }
        $entries.Add([pscustomobject]@{ Original = $original; Info = $info; Path = $relative; Eol = $mode; Hash = (Get-SourceDigest $bytes) })
    }
    if ($entries.Count -eq 0) { throw 'No checksum-bearing project sources found. Check RepositoryRoot and PDB debug information.' }
    $validated = $true

    $stream = [Collections.Generic.List[string]]::new()
    $stream.Add('SRCSRV: ini ------------------------------------------------')
    $stream.Add('VERSION=1')
    $stream.Add('INDEXVERSION=2')
    $stream.Add('VERCTRL=Git')
    $stream.Add('SRCSRV: variables ------------------------------------------')
    $stream.Add('SRCSRVTRG=%targ%\hitori-git\%var2%\%var4%\%var6%\%fnbksl%(%var5%)')
    $stream.Add('SRCSRVCMD=%HITORI_GIT_SOURCE_COMMAND%')
    $stream.Add('SRCSRVENV=HITORI_GIT_DATA=|%var2%|%var3%|%var4%|%var5%|%var6%|%var7%|%srcsrvtrg%')
    $stream.Add('SRCSRV: source files ---------------------------------------')
    foreach ($entry in $entries) {
        $stream.Add("$($entry.Original)*$($entry.Info.Key)*$($entry.Info.Remote)*$($entry.Info.Commit)*$($entry.Path)*$($entry.Eol)*$($entry.Hash)")
    }
    $stream.Add('SRCSRV: end ------------------------------------------------')
    $temporaryStream = [IO.Path]::GetTempFileName()
    # Native SrcSrv tools use the Windows ANSI code page, not a UTF-8 BOM.
    $encoding = [Text.Encoding]::Default
    $streamText = ($stream -join "`r`n") + "`r`n"
    if ($encoding.GetString($encoding.GetBytes($streamText)) -cne $streamText) { throw 'Source paths cannot be represented in the Windows ANSI code page.' }
    [IO.File]::WriteAllText($temporaryStream, $streamText, $encoding)
    $temporaryPdb = Join-Path ([IO.Path]::GetDirectoryName($pdb)) ([Guid]::NewGuid().ToString('N') + '.pdb')
    [IO.File]::Copy($pdb, $temporaryPdb)
    & $pdbStr -w "-p:$temporaryPdb" "-i:$temporaryStream" -s:srcsrv
    if ($LASTEXITCODE -ne 0) { throw 'Failed to write PDB srcsrv stream.' }
    & $pdbStr -r "-p:$temporaryPdb" "-i:$temporaryStream" -s:srcsrv
    if ($LASTEXITCODE -ne 0) { throw 'Failed to read back PDB srcsrv stream.' }
    if ([IO.File]::ReadAllText($temporaryStream, $encoding) -cne $streamText) { throw 'PDB srcsrv verification failed.' }
    [IO.File]::Copy($temporaryPdb, $pdb, $true)
    Write-Host "[Source indexing] $($entries.Count) project files; $externalCount external SDK entries, $thirdPartyCount third-party entries and $generatedCount generated files excluded."
    Write-Host '[Source indexing] Git commit metadata only. Push all indexed commits before distributing this build.'
} catch {
    Write-Error $_ -ErrorAction Continue
    if (-not $validated) { exit 2 }
    exit 1
} finally {
    if ($temporaryStream -and (Test-Path -LiteralPath $temporaryStream)) { Remove-Item -LiteralPath $temporaryStream }
    if ($temporaryPdb -and (Test-Path -LiteralPath $temporaryPdb)) { Remove-Item -LiteralPath $temporaryPdb }
}
