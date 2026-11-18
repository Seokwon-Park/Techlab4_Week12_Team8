# Shared byte-preserving Git helpers for indexing and debugger-side retrieval.
function Invoke-SourceGit([string]$Directory, [string[]]$GitArguments) {
    $start = New-Object Diagnostics.ProcessStartInfo
    $start.FileName = 'git.exe'
    $start.WorkingDirectory = $Directory
    $start.UseShellExecute = $false
    $start.CreateNoWindow = $true
    $start.RedirectStandardOutput = $true
    $start.RedirectStandardError = $true
    $start.EnvironmentVariables['GIT_TERMINAL_PROMPT'] = '0'
    $start.Arguments = (@($GitArguments | ForEach-Object {
        if ($_ -match '["\r\n]' -or $_.EndsWith('\')) { throw 'Unsupported Git argument.' }
        '"' + $_ + '"'
    }) -join ' ')
    $process = [Diagnostics.Process]::Start($start)
    $errorTask = $process.StandardError.ReadToEndAsync()
    $memory = New-Object IO.MemoryStream
    try {
        $process.StandardOutput.BaseStream.CopyTo($memory)
        $process.WaitForExit()
        $errorText = $errorTask.Result
        if ($process.ExitCode -ne 0) { throw "Git failed: $errorText" }
        return ,$memory.ToArray()
    } finally { $memory.Dispose(); $process.Dispose() }
}
function Get-SourceDigest([byte[]]$Bytes, [string]$Algorithm = 'SHA256') {
    $hasher = [Security.Cryptography.HashAlgorithm]::Create($Algorithm)
    try { [BitConverter]::ToString($hasher.ComputeHash($Bytes)).Replace('-', '').ToLowerInvariant() }
    finally { $hasher.Dispose() }
}
function Convert-SourceEol([byte[]]$Bytes, [string]$Mode) {
    if ($Mode -eq 'raw') { return ,$Bytes }
    # Latin-1 is a reversible byte mapping, including UTF-8 and BOM bytes.
    $encoding = [Text.Encoding]::GetEncoding(28591)
    return ,$encoding.GetBytes($encoding.GetString($Bytes).Replace("`r`n", "`n").Replace("`n", "`r`n"))
}
