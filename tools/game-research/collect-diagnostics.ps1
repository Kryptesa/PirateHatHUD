# Developer utility: archives only explicitly selected files.
[CmdletBinding()]
param(
  [Parameter(Mandatory = $true)]
  [ValidateNotNullOrEmpty()]
  [string[]]$Files,
  [Parameter(Mandatory = $true)]
  [ValidateNotNullOrEmpty()]
  [string]$OutputZip,
  [string]$GameVersion = '',
  [string]$ModVersion = ''
)

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'
Add-Type -AssemblyName System.IO.Compression

$selected = @()
foreach ($source in $Files) {
  $item = Get-Item -LiteralPath $source
  if ($item -isnot [System.IO.FileInfo]) {
    throw "Select individual files, not directories: $source"
  }
  $selected += $item
}
$destination = $ExecutionContext.SessionState.Path.GetUnresolvedProviderPathFromPSPath($OutputZip)
if ([System.IO.Path]::GetExtension($destination) -ne '.zip') {
  throw 'OutputZip must have a .zip extension.'
}
if (Test-Path -LiteralPath $destination) {
  throw 'OutputZip already exists. Choose a new filename.'
}
if (-not (Test-Path -LiteralPath ([System.IO.Path]::GetDirectoryName($destination)) -PathType Container)) {
  throw 'The output directory must already exist.'
}

$outputStream = $null
$archive = $null
$created = $false
try {
  # CreateNew also protects against another process creating the output after the check.
  $outputStream = [System.IO.File]::Open($destination, [System.IO.FileMode]::CreateNew)
  $created = $true
  $archive = [System.IO.Compression.ZipArchive]::new(
    $outputStream, [System.IO.Compression.ZipArchiveMode]::Create, $true)
  $records = @()
  $index = 0
  foreach ($item in $selected) {
    $index++
    # Numbered basenames prevent collisions; source directories are never recorded.
    $entryName = 'files/{0:D3}-{1}' -f $index, $item.Name
    $inputStream = $null
    $entryStream = $null
    $hasher = $null
    try {
      $inputStream = [System.IO.File]::Open(
        $item.FullName, [System.IO.FileMode]::Open, [System.IO.FileAccess]::Read,
        [System.IO.FileShare]::Read)
      $size = $inputStream.Length
      $hasher = [System.Security.Cryptography.SHA256]::Create()
      $hash = [System.BitConverter]::ToString($hasher.ComputeHash($inputStream)).Replace('-', '').ToLowerInvariant()
      $inputStream.Position = 0
      $entry = $archive.CreateEntry($entryName)
      $entryStream = $entry.Open()
      $inputStream.CopyTo($entryStream)
      $records += [ordered]@{ archive_name = $entryName; size_bytes = $size; sha256 = $hash }
    } finally {
      if ($null -ne $entryStream) { $entryStream.Dispose() }
      if ($null -ne $inputStream) { $inputStream.Dispose() }
      if ($null -ne $hasher) { $hasher.Dispose() }
    }
  }
  $manifest = [ordered]@{
    schema_version = 1
    tool = 'collect-diagnostics'
    collected_utc = [DateTime]::UtcNow.ToString('o')
    game_version = $GameVersion
    mod_version = $ModVersion
    files = @($records)
  }
  $manifestEntry = $archive.CreateEntry('manifest.json')
  $writer = [System.IO.StreamWriter]::new($manifestEntry.Open(), [System.Text.UTF8Encoding]::new($false))
  try { $writer.Write(($manifest | ConvertTo-Json -Depth 10)) } finally { $writer.Dispose() }
  $archive.Dispose()
  $archive = $null
  $outputStream.Dispose()
  $outputStream = $null
  Write-Output ('Created {0} with {1} selected files.' -f [System.IO.Path]::GetFileName($destination), $selected.Count)
} catch {
  if ($null -ne $archive) { $archive.Dispose(); $archive = $null }
  if ($null -ne $outputStream) { $outputStream.Dispose(); $outputStream = $null }
  if ($created) { Remove-Item -LiteralPath $destination }
  throw
} finally {
  if ($null -ne $archive) { $archive.Dispose() }
  if ($null -ne $outputStream) { $outputStream.Dispose() }
}
