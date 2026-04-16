from PIL import Image
from argparse import ArgumentParser

parser = ArgumentParser(description="Convert an image to RGB565 format and generate a C header file")
parser.add_argument("input", help="Path to the input image (e.g. src/img/background.png)")
parser.add_argument("output", help="Path to the output header file (e.g. src/img/background.h)")
parser.add_argument("--width", type=int, default=None, help="Width to resize the image to (default calculated from input image)")
parser.add_argument("--height", type=int, default=None, help="Height to resize the image to (default calculated from input image)")
args = parser.parse_args()

img = Image.open(args.input).convert("RGB")
if args.width is None:
    args.width = img.width
if args.height is None:
    args.height = img.height

if img.width != args.width or img.height != args.height:
    print(f"Resizing image from {img.width}x{img.height} to {args.width}x{args.height}")
    img = img.resize((args.width, args.height), Image.LANCZOS)

pixels = list(img.get_flattened_data())

def to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)

values = [to_rgb565(r, g, b) for r, g, b in pixels]

lines = [
    "#pragma once",
    "#include <stdint.h>",
    "",
    f"// Auto-generated from {args.input} -- {args.width}x{args.height} RGB565",
    f"// {args.width*args.height} pixels, {args.width*args.height*2} bytes stored in flash",
    f"static const uint16_t {args.output.split('/')[-1].split('.')[0]}[{args.width*args.height}] = {{",
]

row_size = 16
for i in range(0, len(values), row_size):
    chunk = values[i:i+row_size]
    lines.append("    " + ", ".join(f"0x{v:04X}" for v in chunk) + ",")

lines += ["};", ""]

with open(args.output, "w") as f:
    f.write("\n".join(lines))

print(f"OK: {len(values)} pixels, {len(values)*2} bytes -> {args.output}")