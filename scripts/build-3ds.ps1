param([switch]$Private)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$sdk=if($env:DEVKITPRO -and (Test-Path (Join-Path $env:DEVKITPRO 'devkitARM/bin/arm-none-eabi-gcc.exe'))){$env:DEVKITPRO}else{'C:/devkitPro'}
$gcc="$sdk/devkitARM/bin/arm-none-eabi-gcc.exe"
$gxx="$sdk/devkitARM/bin/arm-none-eabi-g++.exe"
$out=Join-Path $repo $(if($Private){'build/private/3ds'}else{'build/3ds'})
$artifactName=if($Private){'PixelDoodle-Private'}else{'PixelDoodle'}
New-Item -ItemType Directory -Force $out | Out-Null
$flags=@('-std=gnu11','-O2','-g','-Wall','-Wextra','-Werror','-Wno-misleading-indentation','-march=armv6k','-mtune=mpcore','-mfloat-abi=hard','-mtp=soft','-mword-relocations','-ffunction-sections','-fstack-usage','-D__3DS__',"-I$repo/vendor/zlib","-I$repo/core","-I$repo/ui","-I$sdk/libctru/include")
if($Private){$flags+= '-DPD_PRIVATE_BUILD=1'}
if($Private){& "$PSScriptRoot/prepare-private-webhook.ps1"}
$objects=@()
foreach($src in @('core/pixel.c','core/animation.c','core/gif.c','core/storage.c','core/files.c','core/community.c','ui/dropdown.c','ui/app.c','platform/3ds/main.c','platform/3ds/pairing.c') + @(Get-ChildItem "$repo/vendor/zlib/*.c" | ForEach-Object { 'vendor/zlib/'+$_.Name })) {
    $obj=Join-Path $out (([IO.Path]::GetFileNameWithoutExtension($src))+'.o')
    & $gcc @flags -c (Join-Path $repo $src) -o $obj
    if($LASTEXITCODE -ne 0){throw "Compile failed: $src"}
    $objects += $obj
}
# The community tick runs on the small default main-thread stack. A 32 KiB
# automatic response buffer previously caused a hardware data abort in threadFree.
$tickUsage=Get-Content "$out/pairing.su" | Where-Object { $_ -match ':pairing_tick_impl\s' }
if(!$tickUsage -or [int](($tickUsage -split "`t")[1]) -gt 4096){throw 'Community main-thread stack frame exceeds 4 KiB budget'}
# Embed the same Mozilla CA bundle used by the NDS software TLS transport.
$ca=[IO.File]::ReadAllBytes("$repo/data/cacert.pem")
$header='static const unsigned char cacert_pem[]={' + (($ca | ForEach-Object { [string]$_ }) -join ',') + '};' + "`n" + 'static const unsigned int cacert_pem_size=sizeof(cacert_pem);'
[IO.File]::WriteAllText("$out/cacert_pem.h",$header)
$tlsFlags=@("-I$repo/vendor/mbedtls/include","-I$repo/platform/3ds","-I$out",'-DMBEDTLS_CONFIG_FILE=<doodle_mbedtls_config.h>')
foreach($src in Get-ChildItem "$repo/vendor/mbedtls/library/*.c") {
    $obj=Join-Path $out ('tls_'+$src.BaseName+'.o')
    & $gcc @flags @tlsFlags -c $src.FullName -o $obj
    if($LASTEXITCODE -ne 0){throw "TLS compile failed: $src"}
    $objects += $obj
}
$cxxFlags=@($flags | Where-Object {$_ -ne '-std=gnu11'}) + @('-std=gnu++11','-fno-exceptions','-fno-rtti')
foreach($name in @('tls_stream','https_client','community_https')) {
    $obj=Join-Path $out ($name+'.o')
    & $gxx @cxxFlags @tlsFlags -c "$repo/platform/3ds/$name.cpp" -o $obj
    if($LASTEXITCODE -ne 0){throw "C++ compile failed: $name"}
    $objects += $obj
}
& $gxx '-specs=3dsx.specs' -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft "-L$sdk/libctru/lib" @objects -lctru -lm "-Wl,-Map,$out/$artifactName.map" -o "$out/$artifactName.elf"
if($LASTEXITCODE -ne 0){throw '3DS link failed'}
$version=[regex]::Match((Get-Content "$repo/core/version.h" -Raw),'PD_VERSION "([^"]+)"').Groups[1].Value
& "$sdk/tools/bin/smdhtool.exe" --create $(if($Private){'Pixel Doodle Private'}else{'Pixel Doodle'}) "Offline pixel art $version" 'Tommy' "$repo/assets/icon-3ds.png" "$out/$artifactName.smdh"
if($LASTEXITCODE -ne 0){throw 'SMDH failed'}
& "$sdk/tools/bin/3dsxtool.exe" "$out/$artifactName.elf" "$out/$artifactName.3dsx" "--smdh=$out/$artifactName.smdh"
if($LASTEXITCODE -ne 0){throw '3DSX failed'}
if(!$Private){& "$PSScriptRoot/build-cia.ps1" -Sdk $sdk -Output $out}
