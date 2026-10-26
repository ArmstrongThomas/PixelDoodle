#!/usr/bin/env bash
# Builds the standard Pixel Doodle 3DSX inside a devkitPro image. CIA packaging is separate
# (scripts/build-cia-unix.sh) because makerom/bannertool are host tools, not devkitArm tools.
set -euo pipefail
repo=$(cd "$(dirname "${BASH_SOURCE[0]}")/.." && pwd)
cd "$repo"
sdk=${DEVKITPRO:-/opt/devkitpro}
gcc="$sdk/devkitARM/bin/arm-none-eabi-gcc"
gxx="$sdk/devkitARM/bin/arm-none-eabi-g++"
[ -x "$gcc" ] && [ -x "$gxx" ] || { echo "devkitARM not found under $sdk" >&2; exit 1; }
out=build/3ds
mkdir -p "$out"

flags=(-std=gnu11 -O2 -g -Wall -Wextra -Werror -Wno-misleading-indentation
  -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft -mword-relocations
  -ffunction-sections -fstack-usage -D__3DS__
  -I"$repo/vendor/zlib" -I"$repo/core" -I"$repo/ui" -I"$sdk/libctru/include")
objects=()

for src in core/pixel.c core/animation.c core/ase_export.c core/gif.c core/storage.c \
           core/files.c core/community.c ui/dropdown.c ui/app.c \
           platform/3ds/main.c platform/3ds/pairing.c; do
  obj="$out/$(basename "${src%.c}").o"
  "$gcc" "${flags[@]}" -c "$src" -o "$obj"
  objects+=("$obj")
done
for src in vendor/zlib/*.c; do
  obj="$out/$(basename "${src%.c}").o"
  "$gcc" "${flags[@]}" -c "$src" -o "$obj"
  objects+=("$obj")
done

# Embed the same Mozilla CA bundle as the Windows build; the file is generated, not tracked.
python3 - "$repo" <<'PY'
import pathlib, sys
repo = pathlib.Path(sys.argv[1])
ca = (repo / "data" / "cacert.pem").read_bytes()
text = "static const unsigned char cacert_pem[]={" + ",".join(str(b) for b in ca) + "};\n"
text += "static const unsigned int cacert_pem_size=sizeof(cacert_pem);\n"
(repo / "build" / "3ds" / "cacert_pem.h").write_text(text)
PY

# The community tick runs on the small main-thread stack; reject an oversized frame.
bytes=$(awk -F'\t' '$1 ~ /:pairing_tick_impl$/ { print $2 }' "$out/pairing.su")
if [ -z "$bytes" ] || [ "$bytes" -gt 4096 ]; then
  echo "Community main-thread stack frame is missing or exceeds 4 KiB ($bytes)" >&2
  exit 1
fi

tlsflags=(-I"$repo/vendor/mbedtls/include" -I"$repo/platform/3ds" -I"$out"
  '-DMBEDTLS_CONFIG_FILE=<doodle_mbedtls_config.h>')
for src in vendor/mbedtls/library/*.c; do
  obj="$out/tls_$(basename "${src%.c}").o"
  "$gcc" "${flags[@]}" "${tlsflags[@]}" -c "$src" -o "$obj"
  objects+=("$obj")
done

cxxflags=(-std=gnu++11 -fno-exceptions -fno-rtti)
for f in "${flags[@]}"; do
  [ "$f" = "-std=gnu11" ] || cxxflags+=("$f")
done
for name in tls_stream https_client community_https; do
  obj="$out/$name.o"
  "$gxx" "${cxxflags[@]}" "${tlsflags[@]}" -c "platform/3ds/$name.cpp" -o "$obj"
  objects+=("$obj")
done

"$gxx" -specs=3dsx.specs -march=armv6k -mtune=mpcore -mfloat-abi=hard -mtp=soft \
  -L"$sdk/libctru/lib" "${objects[@]}" -lctru -lm \
  -Wl,-Map,"$out/PixelDoodle.map" -o "$out/PixelDoodle.elf"

version=$(sed -n 's/^#define PD_VERSION "\(.*\)"/\1/p' core/version.h | tr -d '\r')
smdhtool --create "Pixel Doodle" "Offline pixel art $version" "Tommy" \
  assets/icon-3ds.png "$out/PixelDoodle.smdh"
3dsxtool "$out/PixelDoodle.elf" "$out/PixelDoodle.3dsx" "--smdh=$out/PixelDoodle.smdh"
echo "Built $out/PixelDoodle.3dsx for $version"
