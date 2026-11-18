<#
Indexes native PDBs against checksum-verified source snapshots beside the symbol store.
Run after linking and before symstore add. Requires Windows SDK Debugging Tools.
#>
[CmdletBinding()]
param(
    [Parameter(Mandatory = $true)][string]$PdbPath,
    [Parameter(Mandatory = $true)][string]$SourceRoot,
    [string]$RepositoryRoot,
    [string]$ToolsDirectory = "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\srcsrv"
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
$temporaryStream = $null
$temporaryPdb = $null
$validated = $false
function Get-SourceHash([string]$Path) {
    $sha = [Security.Cryptography.SHA256]::Create()
    $inputStream = [IO.File]::OpenRead($Path)
    try { [BitConverter]::ToString($sha.ComputeHash($inputStream)).Replace('-', '').ToLowerInvariant() }
    finally { $inputStream.Dispose(); $sha.Dispose() }
}
try {
    if (-not $RepositoryRoot) { $RepositoryRoot = Split-Path $PSScriptRoot -Parent }
    $pdb = (Resolve-Path -LiteralPath $PdbPath).Path
    $repository = (Resolve-Path -LiteralPath $RepositoryRoot).Path.TrimEnd('\') + '\'
    $sourceStore = [IO.Path]::GetFullPath($SourceRoot).TrimEnd('\')
    # SrcSrv interprets these characters as delimiters or variable references.
    if ($sourceStore -match '[%*\r\n]') { throw 'SourceRoot contains SrcSrv reserved characters.' }
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
    $buildArtifactCount = 0
    $thirdPartyRoot = Join-Path $repository 'Source\ThirdParty\'
    $buildRoot = Join-Path $repository 'Build\'
    foreach ($line in $rawSources) {
        if ($line -notmatch '^(.*?)\t\s*Checksum (MD5|SHA1|SHA256):\s*([0-9A-Fa-f]+)\s*$') { continue }
        $original = $Matches[1]
        $algorithm = $Matches[2]
        $expected = $Matches[3]
        if (-not $original.StartsWith($repository, [StringComparison]::OrdinalIgnoreCase)) {
            $externalCount++
            continue
        }
        # Keep external libraries and linker inputs (.res, etc.) out of the source store.
        if ($original.StartsWith($thirdPartyRoot, [StringComparison]::OrdinalIgnoreCase)) {
            $thirdPartyCount++
            continue
        }
        if ($original.StartsWith($buildRoot, [StringComparison]::OrdinalIgnoreCase)) {
            $buildArtifactCount++
            continue
        }
        if ($original -match '[%*\r\n]') { throw "Unsupported source path: $original" }
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
        $sha = [Security.Cryptography.SHA256]::Create()
        try { $contentId = [BitConverter]::ToString($sha.ComputeHash($bytes)).Replace('-', '').ToLowerInvariant() }
        finally { $sha.Dispose() }
        $name = [IO.Path]::GetFileName($original)
        $entries.Add([pscustomobject]@{ Original = $original; Hash = $contentId; Name = $name; Bytes = $bytes })
    }
    if ($entries.Count -eq 0) { throw 'No checksum-bearing project sources found. Check RepositoryRoot and PDB debug information.' }
    $validated = $true

    # Validate every file before writing snapshots or changing the PDB.
    foreach ($entry in $entries) {
        $directory = Join-Path $sourceStore $entry.Hash
        [IO.Directory]::CreateDirectory($directory) | Out-Null
        $target = Join-Path $directory $entry.Name
        if (Test-Path -LiteralPath $target) {
            if ((Get-SourceHash $target) -ne $entry.Hash) {
                throw "Source snapshot is corrupt: $target"
            }
        } else {
            # A unique staging file prevents readers from seeing partial source files.
            $staging = Join-Path $directory ([Guid]::NewGuid().ToString('N') + '.tmp')
            try {
                [IO.File]::WriteAllBytes($staging, $entry.Bytes)
                try { [IO.File]::Move($staging, $target) }
                catch {
                    if (-not (Test-Path -LiteralPath $target) -or
                        (Get-SourceHash $target) -ne $entry.Hash) { throw }
                }
            } finally {
                if (Test-Path -LiteralPath $staging) { Remove-Item -LiteralPath $staging }
            }
        }
    }

    # Direct file/UNC targets need no command execution, Git checkout, or credentials in PDBs.
    $stream = [Collections.Generic.List[string]]::new()
    $stream.Add('SRCSRV: ini ------------------------------------------------')
    $stream.Add('VERSION=2')
    $stream.Add('INDEXVERSION=2')
    $stream.Add('VERCTRL=HitoriSourceSnapshot')
    $stream.Add('SRCSRV: variables ------------------------------------------')
    $stream.Add("HITORI_SOURCE_ROOT=$sourceStore")
    $stream.Add('SRCSRVTRG=%HITORI_SOURCE_ROOT%\%var2%\%var3%')
    $stream.Add('SRCSRVCMD=')
    $stream.Add('SRCSRV: source files ---------------------------------------')
    foreach ($entry in $entries) { $stream.Add("$($entry.Original)*$($entry.Hash)*$($entry.Name)") }
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
    Write-Host "[Source indexing] $($entries.Count) project files; $externalCount external SDK entries, $thirdPartyCount third-party entries and $buildArtifactCount build artifacts excluded."
    Write-Host "[Source indexing] $sourceStore"
    if (-not $sourceStore.StartsWith('\\')) {
        Write-Warning 'Local source store: other PCs need access to this path or a HITORI_SOURCE_ROOT override in srcsrv.ini.'
    }
} catch {
    Write-Error $_ -ErrorAction Continue
    if (-not $validated) { exit 2 }
    exit 1
} finally {
    if ($temporaryStream -and (Test-Path -LiteralPath $temporaryStream)) { Remove-Item -LiteralPath $temporaryStream }
    if ($temporaryPdb -and (Test-Path -LiteralPath $temporaryPdb)) { Remove-Item -LiteralPath $temporaryPdb }
}
