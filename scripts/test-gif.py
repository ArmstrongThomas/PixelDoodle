"""Decode native exporter fixtures with Pillow, independently of the encoder."""
import struct
import sys
from pathlib import Path
from PIL import Image

root = Path(sys.argv[1])
image = Image.open(root / "animation.gif")
assert image.n_frames == 3 and image.info["loop"] == 0
for frame, duration in enumerate((110, 250, 500)):
    image.seek(frame)
    assert image.info["duration"] == duration
    values = struct.unpack("<16384I", (root / f"gif-frame-{frame}.rgba").read_bytes())
    for actual, expected in zip(struct.iter_unpack("4B", image.convert("RGBA").tobytes()), values):
        assert actual[3] == expected >> 24
        if expected >> 24:
            assert actual[:3] == ((expected >> 16) & 255, (expected >> 8) & 255, expected & 255)

image = Image.open(root / "quantized.gif").convert("RGBA")
for i, pixel in enumerate(struct.iter_unpack("4B", image.tobytes())):
    if i == 0:
        assert pixel[3] == 0
    elif i == 1:
        assert pixel == (255, 0, 0, 255)
    else:
        color = 0xFF000000 | i * 1777
        expected = tuple((((color >> shift) & 255) + 25) // 51 * 51 for shift in (16, 8, 0))
        assert pixel == (*expected, 255)
print("Independent GIF decoder: exact palettes, fallback colors, disposal/transparency, looping and durations: PASS")
