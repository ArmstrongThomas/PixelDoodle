#!/usr/bin/env bash
# Packages build/3ds/PixelDoodle.cia from the ELF/SMDH produced by build-3ds-unix.sh.
# Requires bannertool and makerom in PATH and build/3ds/banner.png + banner.wav.
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo"
out=build/3ds
for tool in bannertool makerom; do
  command -v "$tool" >/dev/null || { echo "$tool is required in PATH" >&2; exit 1; }
done
for f in "$out/PixelDoodle.elf" "$out/PixelDoodle.smdh" "$out/banner.png" "$out/banner.wav"; do
  [ -f "$f" ] || { echo "missing $f" >&2; exit 1; }
done
version=$(sed -n 's/^#define PD_VERSION "\(.*\)"/\1/p' core/version.h | tr -d '\r')
IFS=. read -r major minor patch <<<"$version"
if [ "$major" -gt 63 ] || [ "$minor" -gt 63 ] || [ "$patch" -gt 15 ]; then
  echo "Version $version cannot be packed into CIA metadata" >&2
  exit 1
fi
title_version=$(( (10#$major << 10) | (10#$minor << 4) | (10#$patch) ))

bannertool makebanner -i "$out/banner.png" -a "$out/banner.wav" -o "$out/banner.bin"
makerom -f cia -target t -ver "$title_version" -exefslogo \
  -o "$out/PixelDoodle.cia" -elf "$out/PixelDoodle.elf" \
  -rsf platform/3ds/pixel.rsf -banner "$out/banner.bin" -icon "$out/PixelDoodle.smdh"
echo "Packaged $out/PixelDoodle.cia (title version $title_version)"
