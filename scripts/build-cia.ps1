param([string]$Sdk='C:/devkitPro',[string]$Output)
$ErrorActionPreference='Stop'
$repo=Split-Path -Parent $PSScriptRoot
$versionText=[regex]::Match((Get-Content "$repo/core/version.h" -Raw),'PD_VERSION "([^"]+)"').Groups[1].Value
$parts=$versionText.Split('.') | ForEach-Object { [int]$_ }
if($parts.Count -ne 3 -or $parts[0] -gt 63 -or $parts[1] -gt 63 -or $parts[2] -gt 15){throw 'Version cannot be packed into CIA metadata'}
$titleVersion=($parts[0] -shl 10) -bor ($parts[1] -shl 4) -bor $parts[2]
# New experimental title ID, deliberately distinct from all Doodle IDs.
& python "$PSScriptRoot/banner.py" $Output
if($LASTEXITCODE -ne 0){throw 'Banner generation failed'}
& "$Sdk/tools/bin/bannertool.exe" makebanner -i "$Output/banner.png" -a "$Output/banner.wav" -o "$Output/banner.bin"
if($LASTEXITCODE -ne 0){throw 'Banner packaging failed'}
& "$Sdk/tools/bin/makerom.exe" -f cia -target t -ver $titleVersion -exefslogo -o "$Output/PixelDoodle.cia" -elf "$Output/PixelDoodle.elf" -rsf "$repo/platform/3ds/pixel.rsf" -banner "$Output/banner.bin" -icon "$Output/PixelDoodle.smdh"
if($LASTEXITCODE -ne 0){throw 'CIA packaging failed'}
& python "$PSScriptRoot/verify-cia.py" "$Output/PixelDoodle.cia"
if($LASTEXITCODE -ne 0){throw 'CIA identity validation failed'}
