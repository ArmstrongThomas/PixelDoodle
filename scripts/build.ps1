param([ValidateSet('all','3ds','nds')][string]$Target='all')
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
Push-Location $repo
try {
    & "$PSScriptRoot/test-host.ps1"
    if ($Target -in @('all','3ds')) {
        & "$PSScriptRoot/build-3ds.ps1"
    }
    if ($Target -in @('all','nds')) {
        $image='skylyrac/blocksds@sha256:779f6193d1030c62e60d1f0f39c3e32cf4045c0bd224a45c0ceadf9d9ab01a51'
        & docker run --rm --network none --mount "type=bind,source=$repo,target=/work" --workdir /work $image make -f Makefile.nds -j2
        if ($LASTEXITCODE -ne 0) { throw 'NDS build failed.' }
        & python scripts/verify-nds.py PixelDoodle.nds
        if ($LASTEXITCODE -ne 0) { throw 'NDS verification failed.' }
    }
    Get-ChildItem -Path PixelDoodle.nds,build/3ds/PixelDoodle.3dsx,build/3ds/PixelDoodle.cia -ErrorAction SilentlyContinue | Get-FileHash -Algorithm SHA256
} finally { Pop-Location }
