# Create a submission archive using an explicit allowlist, never build/.
# Run from Windows: powershell -File scripts/preparar_entrega.ps1
# Existing archives are preserved; archive/move them before generating again.
[CmdletBinding()]
param()

Set-StrictMode -Version Latest
$ErrorActionPreference = 'Stop'

# Resolve the repository from this script, not from the caller's directory.
$labRoot = (Resolve-Path -LiteralPath (Join-Path $PSScriptRoot '..')).Path
$labArchiveDirectory = Join-Path $labRoot 'entrega'
$labArchivePath = Join-Path $labArchiveDirectory 'Laboratorio-3_Daniel-Barillas_22193_CPD_Sec-40.zip'

# Required entries preserve their relative paths for LaTeX and make.
$labEntries = @(
    'README.md', 'AUDITORIA.md', 'LICENSE', 'Makefile',
    'src/01_ping_pong.c', 'src/02_token_ring.c',
    'src/03_recepcion_anticipada.c', 'src/04_pipeline_chunks.c',
    'src/common.h', 'src/visual.h',
    'scripts/mediciones.sh', 'scripts/analizar_resultados.py',
    'scripts/graficas_resultados.py', 'scripts/preparar_entrega.ps1',
    'resultados/ping_pong.csv', 'resultados/token_ring.csv',
    'resultados/recepcion_anticipada.csv', 'resultados/pipeline_chunks.csv',
    'evidencias/01_ping_pong_entero.png', 'evidencias/02_ping_pong_tamanos.png',
    'evidencias/03_token_ring_send_recv.png', 'evidencias/04_token_ring_sendrecv.png',
    'evidencias/05_recepcion_entero.png', 'evidencias/06_recepcion_tamanos.png',
    'evidencias/07_pipeline_chunks.png', 'evidencias/08_pipeline_tamanos.png',
    'informe/informe.tex', 'informe/informe.pdf',
    'informe/figuras/01_ping_pong.pdf', 'informe/figuras/01_ping_pong.png',
    'informe/figuras/02_token_ring.pdf', 'informe/figuras/02_token_ring.png',
    'informe/figuras/03_recepcion_anticipada.pdf', 'informe/figuras/03_recepcion_anticipada.png',
    'informe/figuras/04_pipeline_chunks.pdf', 'informe/figuras/04_pipeline_chunks.png'
)

# Validate every file before creating the archive. Symlinks are not followed.
foreach ($labEntry in $labEntries) {
    $labFile = Get-Item -LiteralPath (Join-Path $labRoot $labEntry)
    if ($labFile.PSIsContainer -or ($labFile.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw "Expected a regular submission file: $labEntry"
    }
}
if (Test-Path -LiteralPath $labArchivePath) {
    throw "Archive already exists; preserve or move it first: $labArchivePath"
}
if (Test-Path -LiteralPath $labArchiveDirectory) {
    $labDirectory = Get-Item -LiteralPath $labArchiveDirectory
    if (-not $labDirectory.PSIsContainer -or ($labDirectory.Attributes -band [IO.FileAttributes]::ReparsePoint)) {
        throw 'entrega must be a regular directory, not a link or file.'
    }
} else {
    New-Item -ItemType Directory -Path $labArchiveDirectory | Out-Null
}

Add-Type -AssemblyName System.IO.Compression
Add-Type -AssemblyName System.IO.Compression.FileSystem

# CreateNew prevents a concurrent invocation from replacing an existing ZIP.
$labStream = [IO.File]::Open($labArchivePath, [IO.FileMode]::CreateNew, [IO.FileAccess]::ReadWrite, [IO.FileShare]::None)
$labArchive = $null
try {
    $labArchive = [IO.Compression.ZipArchive]::new($labStream, [IO.Compression.ZipArchiveMode]::Create, $true)
    foreach ($labEntry in $labEntries) {
        [IO.Compression.ZipFileExtensions]::CreateEntryFromFile(
            $labArchive, (Join-Path $labRoot $labEntry), $labEntry,
            [IO.Compression.CompressionLevel]::Optimal
        ) | Out-Null
    }
} finally {
    if ($null -ne $labArchive) { $labArchive.Dispose() }
    $labStream.Dispose()
}

# Reopen read-only and verify membership. No wildcards can add executables.
$labCheck = [IO.Compression.ZipFile]::OpenRead($labArchivePath)
try {
    $labActual = (@($labCheck.Entries | ForEach-Object { $_.FullName }) | Sort-Object) -join '|'
    $labExpected = ($labEntries | Sort-Object) -join '|'
    if ($labActual -ne $labExpected) { throw 'Archive membership validation failed.' }
} finally {
    $labCheck.Dispose()
}
Write-Output "Submission archive verified: $($labEntries.Count) files, no executables."
Write-Output $labArchivePath
