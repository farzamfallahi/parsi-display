"""Simulate the device: draw shaped glyphs one by one, left to right, on a
128x64 1-bit "OLED" - no text-layout engine involved, exactly like firmware."""
import subprocess, sys
from PIL import Image, ImageDraw, ImageFont

lines = sys.argv[1:-1]
out = sys.argv[-1]
font = ImageFont.truetype("/usr/share/fonts/truetype/dejavu/DejaVuSans.ttf", 15)
oled = Image.new("1", (128, 64), 0)
d = ImageDraw.Draw(oled)
for row, text in enumerate(lines):
    res = subprocess.run(["./build/ps_demo", text], capture_output=True, text=True, check=True)
    cps = [int(t[2:], 16) for t in res.stdout.split() if t.startswith("U+")]
    width = sum(font.getlength(chr(c)) for c in cps)
    x = 128 - 3 - width                      # right-aligned, as RTL text should be
    for c in cps:
        d.text((x, 2 + row * 21), chr(c), font=font, fill=1)
        x += font.getlength(chr(c))
big = oled.convert("L").resize((128 * 5, 64 * 5), Image.NEAREST)
canvas = Image.new("L", (big.width + 20, big.height + 20), 40)
canvas.paste(big, (10, 10))
canvas.save(out)
