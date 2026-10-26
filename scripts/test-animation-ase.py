"""Inspect animated ASE exports; optionally verify them with desktop Aseprite."""
import json
import struct
import subprocess
import sys
from pathlib import Path
from PIL import Image

root = Path(sys.argv[1])
source = root / "animated-layers.aseprite"
data = source.read_bytes()
assert struct.unpack_from("<I5H", data) == (len(data), 0xA5E0, 3, 16, 16, 32)
at = 128
names = []
for frame, duration in enumerate((40, 230, 600)):
    size, magic, chunks, delay = struct.unpack_from("<I3H", data, at)
    assert magic == 0xF1FA and delay == duration
    end = at + size
    at += 16
    cels = 0
    for _ in range(chunks):
        size, kind = struct.unpack_from("<IH", data, at)
        assert size >= 6 and at + size <= end
        payload = data[at + 6:at + size]
        if kind == 0x2004:
            length = struct.unpack_from("<H", payload, 16)[0]
            names.append(payload[18:18 + length].decode())
            layer = len(names) - 1
            assert struct.unpack_from("<H", payload)[0] == (3 if layer == 0 else 1)
            assert payload[12] == (128 if layer == 0 else 255)
            assert struct.unpack_from("<H", payload, 10)[0] == (1 if layer == 0 else 0)
        elif kind == 0x2005:
            layer = struct.unpack_from("<H", payload)[0]
            assert layer == cels
            assert payload[6] == 255 and struct.unpack_from("<H", payload, 7)[0] == 0
            assert struct.unpack_from("<HH", payload, 16) == (16, 16)
            reference = struct.unpack("<256I", (root / f"ase-frame-{frame}-{1-layer}.rgba").read_bytes())
            expected = bytes(v for c in reference for v in ((c >> 16) & 255, (c >> 8) & 255, c & 255, c >> 24))
            assert payload[20:] == expected
            cels += 1
        elif kind == 0x2019:
            assert struct.unpack_from("<III", payload) == (3, 0, 2)
            assert payload[20:] == bytes((0, 0, 0, 0, 0, 0, 0, 0, 0x12, 0x34, 0x56, 255, 0, 0, 0xAB, 0xCD, 0xEF, 128))
        else:
            raise AssertionError(f"Unexpected chunk {kind:x}")
        at += size
    assert at == end and cels == 2
assert at == len(data) and names == ["Shade", "Ink"]
assert source.read_bytes() == source.with_suffix(".aseprite.bak").read_bytes()
print("Independent ASE parser: every RGBA cel, layer order/properties, durations, palette, bounds and backup: PASS")

# Tags (0x2018) and linked cels (cel type 1), including non-contiguous groups.
tagged = root / "tagged-links.aseprite"
data = tagged.read_bytes()
assert struct.unpack_from("<I5H", data) == (len(data), 0xA5E0, 3, 16, 16, 32)
assert struct.unpack_from("<I", data, 14)[0] == 1  # opacity-valid flag
at = 128
tags = []
layers = []
for frame in range(3):
    fsize, magic, old_chunks, delay = struct.unpack_from("<I3H", data, at)
    chunks = old_chunks or struct.unpack_from("<I", data, at + 12)[0]
    assert magic == 0xF1FA and fsize >= 16 and at + fsize <= len(data)
    expected_delay = (120, 240, 240)[frame]
    assert delay == expected_delay, (frame, delay)
    end = at + fsize
    at += 16
    cels = []
    palette = None
    user_colors = []
    for _ in range(chunks):
        size, kind = struct.unpack_from("<IH", data, at)
        assert size >= 6 and at + size <= end
        payload = data[at + 6:at + size]
        if kind == 0x2004:
            layer_flags, layer_type, child, _w, _h, blend = struct.unpack_from("<6H", payload)
            assert layer_type == 0 and child == 0
            name_len = struct.unpack_from("<H", payload, 16)[0]
            layers.append((layer_flags, blend, payload[12], payload[18:18 + name_len].decode()))
        elif kind == 0x2005:
            layer, x, y, opacity, cel_type, z = struct.unpack_from("<HhhBHH", payload)
            assert x == 0 and y == 0 and opacity == 255 and z == 0
            if cel_type == 0:
                w, h = struct.unpack_from("<HH", payload, 16)
                assert w == 16 and h == 16 and len(payload) == 20 + w * h * 4
                cel = ("raw", layer)
            elif cel_type == 1:
                assert len(payload) == 18
                cel = ("linked", layer, struct.unpack_from("<H", payload, 16)[0])
            else:
                raise AssertionError(f"Unexpected cel type {cel_type}")
            cels.append(cel)
        elif kind == 0x2018:
            count = struct.unpack_from("<H", payload)[0]
            assert count == 2 and payload[2:10] == bytes(8)
            pos = 10
            for _tag in range(count):
                frm, to, direction, repeat = struct.unpack_from("<HHBH", payload, pos)
                rgb = tuple(payload[pos + 13:pos + 16])
                assert payload[pos + 16] == 0
                name_len = struct.unpack_from("<H", payload, pos + 17)[0]
                name = payload[pos + 19:pos + 19 + name_len].decode()
                tags.append((name, frm, to, direction, repeat, rgb))
                pos += 19 + name_len
            assert pos == len(payload)
        elif kind == 0x2020:
            flags = struct.unpack_from("<I", payload)[0]
            assert flags == 2 and len(payload) == 8 and payload[7] == 255
            user_colors.append(tuple(payload[4:7]))
        elif kind == 0x2019:
            count, first, last = struct.unpack_from("<3I", payload)
            assert count == 2 and first == 0 and last == 1 and len(payload) == 20 + count * 6
            palette = [tuple(payload[20 + i * 6 + 2:20 + i * 6 + 6]) for i in range(count)]
        else:
            raise AssertionError(f"Unexpected chunk {kind:x}")
        at += size
    assert at == end
    assert len(cels) == 2, (frame, cels)
    if frame < 2:
        assert all(cel[0] == "raw" for cel in cels)
    else:
        assert cels == [("linked", 0, 0), ("linked", 1, 1)], cels
    if frame == 0:
        assert palette == [(0x11, 0x22, 0x33, 0xFF), (0x11, 0x22, 0x33, 0x80)]
        assert user_colors == [(0x12, 0x34, 0x56), (0xAB, 0xCD, 0xEF)], user_colors
assert at == len(data)
assert layers == [(3, 0, 255, "Base"), (3, 1, 160, "Top")], layers
assert tags == [("walk", 0, 1, 0, 0, (0x12, 0x34, 0x56)), ("hold", 1, 2, 2, 3, (0xAB, 0xCD, 0xEF))], tags
print("Independent ASE parser: tags chunk, non-contiguous linked cels, layer/palette properties and frame sizes: PASS")

if len(sys.argv) > 2:
    executable = sys.argv[2]
    sheet, metadata = root / "ase-animation-desktop.png", root / "ase-animation-desktop.json"
    subprocess.run([executable, "--batch", str(source), "--sheet", str(sheet), "--data", str(metadata), "--save-as", str(root / "ase-animation-desktop.aseprite")], check=True, timeout=60)
    output = json.loads(metadata.read_text())
    frames = list(output["frames"].values()) if isinstance(output["frames"], dict) else output["frames"]
    assert [f["duration"] for f in frames] == [40, 230, 600]
    image = Image.open(sheet).convert("RGBA")
    for f in frames:
        assert f["sourceSize"] == {"w": 16, "h": 16}
    for frame, info in enumerate(frames):
        rect = info["frame"]
        actual = image.crop((rect["x"], rect["y"], rect["x"] + rect["w"], rect["y"] + rect["h"]))
        references = [struct.unpack("<256I", (root / f"ase-frame-{frame}-{layer}.rgba").read_bytes()) for layer in (1, 0)]
        for i, pixel in enumerate(struct.iter_unpack("4B", actual.tobytes())):
            alpha, channels = 0, [0, 0, 0]
            for layer, opacity in enumerate((128, 255)):
                color = references[layer][i]
                source_alpha = ((color >> 24) * opacity + 127) // 255
                channels = [((color >> shift) & 255) * source_alpha + (value * (255 - source_alpha) + 127) // 255 for shift, value in zip((16, 8, 0), channels)]
                alpha = source_alpha + (alpha * (255 - source_alpha) + 127) // 255
            assert abs(pixel[3] - alpha) <= 1
            if alpha:
                expected = [(value + alpha // 2) // alpha for value in channels]
                assert all(abs(a - b) <= 2 for a, b in zip(pixel[:3], expected)), (frame, i, pixel, expected)
    f = frames[2]["frame"]
    assert not image.crop((f["x"], f["y"], f["x"] + f["w"], f["y"] + f["h"])).getchannel("A").getbbox()
    print("Desktop Aseprite batch: opens/saves all frames, rendered pixels/alpha within rounding, transparent clearing and durations: PASS")
