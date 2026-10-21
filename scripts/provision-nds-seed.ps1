[CmdletBinding(SupportsShouldProcess=$true, ConfirmImpact='High')]
param(
    [Parameter(Mandatory=$true)]
    [string]$AppDataRoot,
    [switch]$Force
)

$ErrorActionPreference = 'Stop'
$root = [System.IO.Path]::GetFullPath($AppDataRoot)
$root = $root.TrimEnd([char[]]@('\','/'))
if ([System.IO.Path]::GetFileName($root) -ne 'PixelDoodle') {
    throw 'AppDataRoot must be the existing PixelDoodle app-data folder on the target card.'
}
if (-not (Test-Path -LiteralPath $root -PathType Container)) {
    throw "App-data folder does not exist: $root"
}
$private = Join-Path $root 'private'
if (-not (Test-Path -LiteralPath $private -PathType Container)) {
    New-Item -ItemType Directory -Path $private | Out-Null
}
$seed = Join-Path $private 'seed.bin'
$consuming = Join-Path $private 'consuming.bin'
$next = Join-Path $private 'next.bin'
if ((Test-Path -LiteralPath $seed) -and -not $Force) {
    throw 'seed.bin already exists. Use -Force only to recover a card after an interrupted seed rotation.'
}
if ($PSCmdlet.ShouldProcess($private, 'Provision a fresh Pixel Doodle TLS seed and clear interrupted Pixel Doodle seed state')) {
    $bytes = [byte[]]::new(64)
    $rng = [System.Security.Cryptography.RandomNumberGenerator]::Create()
    try { $rng.GetBytes($bytes) } finally { $rng.Dispose() }
    $temp = Join-Path $private 'seed.new'
    [System.IO.File]::WriteAllBytes($temp, $bytes)
    Move-Item -LiteralPath $temp -Destination $seed -Force
    Remove-Item -LiteralPath $consuming,$next -Force -ErrorAction SilentlyContinue
    [Array]::Clear($bytes, 0, $bytes.Length)
    Write-Host 'Pixel Doodle TLS seed provisioned. Keep this card private.'
}
