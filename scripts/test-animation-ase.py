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
