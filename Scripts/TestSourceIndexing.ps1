# Run in an x64 Visual Studio Developer PowerShell. Does not publish symbols.
[CmdletBinding()]
param([string]$ToolsDirectory = "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\srcsrv")
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Get-Command cl.exe -ErrorAction Stop | Out-Null
$repository = Split-Path $PSScriptRoot -Parent
$testRoot = Join-Path $repository ('Build\SourceIndexTests\' + [Guid]::NewGuid().ToString('N'))
$fixture = Join-Path $testRoot 'fixture space'
$store = Join-Path $testRoot 'source store'
[IO.Directory]::CreateDirectory($fixture) | Out-Null
$source = Join-Path $fixture 'probe.cpp'
$header = Join-Path $fixture 'generated.h'
$pdb = Join-Path $fixture 'probe.pdb'
$srcTool = Join-Path $ToolsDirectory 'srctool.exe'
$pdbStr = Join-Path $ToolsDirectory 'pdbstr.exe'
$indexer = Join-Path $PSScriptRoot 'IndexSources.ps1'
function Assert($condition, $message) { if (-not $condition) { throw $message } }
function Hash([string]$path) {
    $sha = [Security.Cryptography.SHA256]::Create()
    try { [BitConverter]::ToString($sha.ComputeHash([IO.File]::ReadAllBytes($path))).Replace('-', '').ToLowerInvariant() }
    finally { $sha.Dispose() }
}
function Index([int]$expectedExit, [string]$root = $store) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $indexer -PdbPath $pdb -SourceRoot $root -RepositoryRoot $fixture -ToolsDirectory $ToolsDirectory
    Assert ($LASTEXITCODE -eq $expectedExit) "Unexpected indexer exit: $LASTEXITCODE; expected $expectedExit"
}
try {
    [IO.File]::WriteAllText($header, '#define PROBE_VALUE 0', [Text.Encoding]::ASCII)
    $original = "#include `"generated.h`"`r`nint main() { return PROBE_VALUE; }`r`n"
    [IO.File]::WriteAllText($source, $original, [Text.Encoding]::ASCII)
    Push-Location $fixture
    try {
        & cl.exe /nologo /Zi /Od /FC probe.cpp /Fe:probe.exe /Fo:probe.obj /Fd:compile.pdb /link /DEBUG /PDB:probe.pdb
        Assert ($LASTEXITCODE -eq 0) 'Fixture compilation failed.'
    } finally { Pop-Location }
    Index 0
    Index 0
    $streamBeforeFailure = @(& $pdbStr -r "-p:$pdb" -s:srcsrv) -join "`n"
    $pdbHashBeforeFailure = Hash $pdb
    $expectedSourceHash = Hash $source
    $snapshot = Join-Path $store ($expectedSourceHash.ToLowerInvariant() + '\probe.cpp')
    Assert (Test-Path -LiteralPath $snapshot) 'Source snapshot missing.'
    Assert ((Hash $snapshot) -eq $expectedSourceHash) 'Source snapshot differs.'
    Assert (@(Get-ChildItem -LiteralPath $store -Recurse -Filter generated.h).Count -eq 1) 'Generated header missing.'

    [IO.File]::WriteAllText($source, $original + '// changed after build', [Text.Encoding]::ASCII)
    Index 2
    $streamAfterFailure = @(& $pdbStr -r "-p:$pdb" -s:srcsrv) -join "`n"
    Assert ($streamBeforeFailure -ceq $streamAfterFailure) 'Failed indexing changed the PDB source stream.'
    Assert ((Hash $pdb) -eq $pdbHashBeforeFailure) 'Failed indexing changed PDB bytes.'
    Move-Item -LiteralPath $source -Destination ($source + '.hidden')
    Index 2
    Assert ((Hash $pdb) -eq $pdbHashBeforeFailure) 'Missing source changed PDB bytes.'
    # Source Server must resolve sources even with the original file unavailable.
    $resolved = @(& $srcTool -x -z $pdb)
    Assert ($LASTEXITCODE -eq 0) 'SrcSrv source retrieval failed.'
    Assert ($resolved -contains $snapshot) 'SrcSrv did not resolve probe.cpp to its snapshot.'
    Assert ((Hash $snapshot) -eq $expectedSourceHash) 'Retrieved source differs.'
    [IO.File]::WriteAllText($source, $original, [Text.Encoding]::ASCII)

    [IO.File]::WriteAllText($snapshot, 'corrupt', [Text.Encoding]::ASCII)
    Index 1
    # Fallback must rewrite the PDB to reference the new source store.
    $fallback = Join-Path $testRoot 'fallback store'
    Index 0 $fallback
    $fallbackStream = @(& $pdbStr -r "-p:$pdb" -s:srcsrv) -join "`n"
    Assert ($fallbackStream.Contains("HITORI_SOURCE_ROOT=$fallback")) 'Fallback path not recorded.'

    # Exercise the actual batch entry point with an isolated symbol store.
    $configuration = 'SourceIndexTest-' + [Guid]::NewGuid().ToString('N')
    $bin = Join-Path $repository "Build\Bin\$configuration-windows-x86_64"
    [IO.Directory]::CreateDirectory($bin) | Out-Null
    Push-Location $fixture
    try {
        & cl.exe /nologo /Zi /Od /FC probe.cpp "/Fe:$bin\HitoriEditor.exe" /Fo:probe.obj /Fd:compile.pdb /link /DEBUG "/PDB:$bin\HitoriEditor.pdb"
        Assert ($LASTEXITCODE -eq 0) 'Publish fixture compilation failed.'
    } finally { Pop-Location }
    $oldStore = $env:HITORI_SYMBOL_STORE
    try {
        $env:HITORI_SYMBOL_STORE = Join-Path $testRoot 'published symbols'
        & (Join-Path $repository 'PublishSymbols.bat') $configuration 'Source indexing test' --no-pause
        Assert ($LASTEXITCODE -eq 0) 'PublishSymbols.bat failed.'
        Assert (Test-Path -LiteralPath "$env:HITORI_SYMBOL_STORE\HitoriEditor.pdb") 'PDB not registered.'
        Assert (Test-Path -LiteralPath "$env:HITORI_SYMBOL_STORE\HitoriEditor.exe") 'EXE not registered.'
        $publishedPdb = @(Get-ChildItem -LiteralPath "$env:HITORI_SYMBOL_STORE\HitoriEditor.pdb" -Recurse -Filter HitoriEditor.pdb)[0]
        $publishedStream = @(& $pdbStr -r "-p:$($publishedPdb.FullName)" -s:srcsrv) -join "`n"
        Assert ($publishedStream.Contains("HITORI_SOURCE_ROOT=$env:HITORI_SYMBOL_STORE\Sources")) 'Published PDB lacks correct source indexing.'
    } finally { $env:HITORI_SYMBOL_STORE = $oldStore }
    Write-Host 'PASS: checksums, generated headers, spaces, repeat indexing, source retrieval, drift/missing/corrupt rejection, fallback, batch publishing.'
    Write-Host "Artifacts: $testRoot"
} finally {
    # Keep all artifacts under ignored Build/ for inspection; no shared store is touched.
}
