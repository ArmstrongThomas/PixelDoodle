$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$sdk=if($env:DEVKITPRO -and (Test-Path (Join-Path $env:DEVKITPRO 'devkitARM/bin/arm-none-eabi-gcc.exe'))){$env:DEVKITPRO}else{'C:/devkitPro'}
$gcc="$sdk/devkitARM/bin/arm-none-eabi-gcc.exe"
$out=Join-Path $repo 'build/3ds'
New-Item -ItemType Directory -Force $out | Out-Null
$flags=@('-std=gnu11','-O2','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-mword-relocations','-ffunction-sections','-D__3DS__',"-I$repo/vendor/zlib","-I$repo/core","-I$repo/ui","-I$sdk/libctru/include")
$objects=@()
foreach($src in @('core/pixel.c','core/storage.c','core/files.c','ui/app.c','platform/3ds/main.c') + @(Get-ChildItem "$repo/vendor/zlib/*.c" | ForEach-Object { 'vendor/zlib/'+$_.Name })) {
    $obj=Join-Path $out (([IO.Path]::GetFileNameWithoutExtension($src))+'.o')
    & $gcc @flags -c (Join-Path $repo $src) -o $obj
    if($LASTEXITCODE -ne 0){throw "Compile failed: $src"}
    $objects += $obj
}
& $gcc '-specs=3dsx.specs' -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft "-L$sdk/libctru/lib" @objects -lctru -lm "-Wl,-Map,$out/PixelDoodle.map" -o "$out/PixelDoodle.elf"
if($LASTEXITCODE -ne 0){throw '3DS link failed'}
$version=[regex]::Match((Get-Content "$repo/core/version.h" -Raw),'PD_VERSION "([^"]+)"').Groups[1].Value
& "$sdk/tools/bin/smdhtool.exe" --create 'Pixel Doodle' "Offline pixel art $version" 'Tommy' "$repo/assets/icon-3ds.png" "$out/PixelDoodle.smdh"
if($LASTEXITCODE -ne 0){throw 'SMDH failed'}
& "$sdk/tools/bin/3dsxtool.exe" "$out/PixelDoodle.elf" "$out/PixelDoodle.3dsx" "--smdh=$out/PixelDoodle.smdh"
if($LASTEXITCODE -ne 0){throw '3DSX failed'}
& "$PSScriptRoot/build-cia.ps1" -Sdk $sdk -Output $out
