param([ValidateSet('all','3ds','nds')][string]$Target='all',[switch]$Private)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
Push-Location $repo
try {
    & "$PSScriptRoot/test-host.ps1"
    if ($Target -in @('all','3ds')) {
        & "$PSScriptRoot/build-3ds.ps1" -Private:$Private
    }
    if ($Target -in @('all','nds')) {
        if($Private){& "$PSScriptRoot/prepare-private-webhook.ps1"}
        & python scripts/verify-nds-tls-assets.py
        if ($LASTEXITCODE -ne 0) { throw 'NDS TLS assets are invalid.' }
        $image='skylyrac/blocksds@sha256:779f6193d1030c62e60d1f0f39c3e32cf4045c0bd224a45c0ceadf9d9ab01a51'
        $privateArg=if($Private){'PD_PRIVATE_BUILD=1'}else{'PD_PRIVATE_BUILD=0'}
        & docker run --rm --network none --mount "type=bind,source=$repo,target=/work" --workdir /work $image make -B -f Makefile.nds -j2 $privateArg
        if ($LASTEXITCODE -ne 0) { throw 'NDS build failed.' }
        $ndsArtifact=if($Private){'PixelDoodle-Private.nds'}else{'PixelDoodle.nds'}
        & python scripts/verify-nds.py $ndsArtifact
        if ($LASTEXITCODE -ne 0) { throw 'NDS verification failed.' }
        if($Private){$privateOut=Join-Path $repo 'build/private/nds';New-Item -ItemType Directory -Force $privateOut | Out-Null;Move-Item -Force (Join-Path $repo $ndsArtifact) (Join-Path $privateOut $ndsArtifact)}
    }
    $artifacts=if($Private){@('build/private/nds/PixelDoodle-Private.nds','build/private/3ds/PixelDoodle-Private.3dsx')}else{@('PixelDoodle.nds','build/3ds/PixelDoodle.3dsx','build/3ds/PixelDoodle.cia')}
    Get-ChildItem -Path $artifacts -ErrorAction SilentlyContinue | Get-FileHash -Algorithm SHA256
} finally { Pop-Location }
