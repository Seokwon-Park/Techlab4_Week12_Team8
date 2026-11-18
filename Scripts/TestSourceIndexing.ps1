# Run in an x64 Visual Studio Developer PowerShell. Uses isolated Git remotes.
[CmdletBinding()]
param([string]$ToolsDirectory = "${env:ProgramFiles(x86)}\Windows Kits\10\Debuggers\x64\srcsrv")
$ErrorActionPreference = 'Stop'
Set-StrictMode -Version Latest
Get-Command cl.exe -ErrorAction Stop | Out-Null
. (Join-Path $PSScriptRoot 'GitSourceTools.ps1')
$testRoot = Join-Path (Split-Path $PSScriptRoot -Parent) ('Build\SourceIndexTests\' + [Guid]::NewGuid().ToString('N'))
$fixture = Join-Path $testRoot 'fixture space'
$remote = Join-Path $testRoot 'remote.git'
$source = Join-Path $fixture 'probe.cpp'
$header = Join-Path $fixture 'tracked.h'
$pdb = Join-Path $fixture 'probe.pdb'
$srcTool = Join-Path $ToolsDirectory 'srctool.exe'
$pdbStr = Join-Path $ToolsDirectory 'pdbstr.exe'
$indexer = Join-Path $PSScriptRoot 'IndexSources.ps1'
function Assert($condition, $message) { if (-not $condition) { throw $message } }
function Git([string[]]$Arguments) { $null = Invoke-SourceGit $fixture $Arguments }
function Index([int]$ExpectedExit) {
    & powershell.exe -NoProfile -ExecutionPolicy Bypass -File $indexer -PdbPath $pdb -RepositoryRoot $fixture -ToolsDirectory $ToolsDirectory
    Assert ($LASTEXITCODE -eq $ExpectedExit) "Indexer exit $LASTEXITCODE; expected $ExpectedExit"
}
function Compile([string]$Name) {
    Push-Location $fixture
    try {
        & rc.exe /nologo /foBuild\Intermediate\EngineProject.res probe.rc
        Assert ($LASTEXITCODE -eq 0) 'Fixture resource compilation failed.'
        & cl.exe /nologo /Zi /Od /FC probe.cpp "/Fe:$Name.exe" "/Fo:$Name.obj" "/Fd:$Name-compile.pdb" /link /DEBUG "/PDB:$Name.pdb" Build\Intermediate\EngineProject.res
        Assert ($LASTEXITCODE -eq 0) 'Fixture compilation failed.'
    } finally { Pop-Location }
}
$saved = @{}
foreach ($name in @('GIT_CONFIG_COUNT', 'GIT_CONFIG_KEY_0', 'GIT_CONFIG_VALUE_0', 'LOCALAPPDATA', 'SRCSRV_INI_FILE', 'HITORI_GIT_SOURCE_COMMAND')) {
    $saved[$name] = [Environment]::GetEnvironmentVariable($name, 'Process')
}
try {
    [IO.Directory]::CreateDirectory($fixture) | Out-Null
    $env:GIT_CONFIG_COUNT = '1'
    $env:GIT_CONFIG_KEY_0 = 'url.' + ([Uri]($remote.Replace('\', '/') + '/')).AbsoluteUri + '.insteadOf'
    $env:GIT_CONFIG_VALUE_0 = 'https://source-index.invalid/fixture.git'
    $env:LOCALAPPDATA = Join-Path $testRoot 'debugger profile'
    Git @('init', '.')
    Git @('config', 'user.name', 'Source Index Test')
    Git @('config', 'user.email', 'source-index@example.invalid')
    Git @('config', 'core.autocrlf', 'true')
    Git @('remote', 'add', 'origin', 'https://source-index.invalid/fixture.git')
    Git @('init', '--bare', $remote)
    [IO.File]::WriteAllText($header, "#define PROBE_VALUE 0`r`n", [Text.Encoding]::ASCII)
    $generatedDirectory = Join-Path $fixture 'Intermediate\Generated\Runtime'
    [IO.Directory]::CreateDirectory($generatedDirectory) | Out-Null
    $generatedHeader = Join-Path $generatedDirectory 'Material.generated.h'
    [IO.File]::WriteAllText($generatedHeader, "#define GENERATED_VALUE 0`r`n", [Text.Encoding]::ASCII)
    $thirdPartyDirectory = Join-Path $fixture 'Source\ThirdParty\ImGui'
    [IO.Directory]::CreateDirectory($thirdPartyDirectory) | Out-Null
    [IO.File]::WriteAllText((Join-Path $thirdPartyDirectory 'imgui.h'), "#define THIRD_PARTY_VALUE 0`r`n", [Text.Encoding]::ASCII)
    $original = "#include `"tracked.h`"`r`n#include `"Intermediate/Generated/Runtime/Material.generated.h`"`r`n#include `"Source/ThirdParty/ImGui/ImGui.h`"`r`nint main() { return PROBE_VALUE + GENERATED_VALUE + THIRD_PARTY_VALUE; }`r`n"
    [IO.File]::WriteAllText($source, $original, [Text.Encoding]::ASCII)
    [IO.Directory]::CreateDirectory((Join-Path $fixture 'Build\Intermediate')) | Out-Null
    [IO.File]::WriteAllText((Join-Path $fixture 'probe.rc'), "1 RCDATA { 1, 2, 3 }`r`n", [Text.Encoding]::ASCII)
    Git @('add', 'probe.cpp', 'tracked.h', 'probe.rc')
    Git @('commit', '-m', 'fixture')
    Git @('push', 'origin', 'HEAD:refs/heads/main')
    Compile 'probe'
    $rawSources = @(& $srcTool -r -h -z $pdb) -join "`n"
    Assert ($rawSources -match 'EngineProject\.res') 'Resource artifact missing from fixture PDB.'
    Index 0
    Index 0
    Assert (-not (Test-Path -LiteralPath $env:LOCALAPPDATA)) 'Indexing fetched Git sources.'
    & (Join-Path $PSScriptRoot 'SetupGitSourceDebugging.ps1') -SessionOnly
    $stream = @(& $pdbStr -r "-p:$pdb" -s:srcsrv) -join "`n"
    Assert ($stream.Contains('VERCTRL=Git')) 'Git provider missing.'
    Assert (-not $stream.Contains('Material.generated.h')) 'Generated header was indexed for Git retrieval.'
    Assert ($stream -notmatch '(?i)imgui\.h') 'Third-party header was indexed for Git retrieval.'
    Assert (-not $stream.Contains('EngineProject.res')) 'Build artifact was indexed for Git retrieval.'
    Assert (-not $stream.Contains('HITORI_SOURCE_ROOT=')) 'Snapshot provider remains.'
    # Trust the extraction executable only for this isolated test process (never a global setting).
    $env:SRCSRV_INI_FILE = Join-Path $testRoot 'srcsrv.ini'
    [IO.File]::WriteAllText($env:SRCSRV_INI_FILE, "[trusted commands]`r`ncmd.exe`r`n", [Text.Encoding]::ASCII)
    $pdbHash = Get-SourceDigest ([IO.File]::ReadAllBytes($pdb))
    $sourceHash = Get-SourceDigest ([IO.File]::ReadAllBytes($source))
    [IO.File]::WriteAllText($source, $original + '// uncommitted', [Text.Encoding]::ASCII)
    Index 2
    Assert ((Get-SourceDigest ([IO.File]::ReadAllBytes($pdb))) -eq $pdbHash) 'Rejected source changed PDB.'
    Compile 'dirty'
    $pdb = Join-Path $fixture 'dirty.pdb'
    Index 2
    $pdb = Join-Path $fixture 'probe.pdb'
    Move-Item -LiteralPath $source -Destination ($source + '.hidden')
    Index 2
    # Advance HEAD; the old PDB must still retrieve the pinned source.
    [IO.File]::WriteAllText($source, 'int main() { return 9; }', [Text.Encoding]::ASCII)
    Git @('add', 'probe.cpp')
    Git @('commit', '-m', 'newer source')
    Git @('push', 'origin', 'HEAD:refs/heads/main')
    Remove-Item -LiteralPath $source
    $cache = Join-Path $testRoot 'source cache space'
    $resolved = @(& $srcTool -x -n -z "-d:$cache" $pdb)
    $resolved | Write-Host
    Assert ($LASTEXITCODE -eq 0) 'SrcSrv Git extraction failed.'
    $downloaded = @(Get-ChildItem -LiteralPath $cache -Recurse -Filter probe.cpp)
    Assert ($downloaded.Count -eq 1) 'Requested source was not retrieved.'
    Assert ((Get-SourceDigest ([IO.File]::ReadAllBytes($downloaded[0].FullName))) -eq $sourceHash) 'Git source differs from build bytes.'
    Move-Item -LiteralPath $remote -Destination ($remote + '.offline')
    $offlineCache = Join-Path $testRoot 'offline source cache'
    $null = & $srcTool -x -z "-d:$offlineCache" $pdb
    Assert ($LASTEXITCODE -eq 0) 'Cached Git commit retrieval failed.'
    Assert (@(Get-ChildItem -LiteralPath $offlineCache -Recurse -Filter tracked.h).Count -eq 1) 'Tracked header missing.'
    Write-Host 'PASS: metadata-only/repeat indexing, drift/dirty/missing rejection, pinned Git retrieval, CRLF, spaces, offline cache.'
    Write-Host "Artifacts: $testRoot"
} finally {
    foreach ($name in $saved.Keys) { [Environment]::SetEnvironmentVariable($name, $saved[$name], 'Process') }
}
