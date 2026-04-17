import re
from PIL import Image
from argparse import ArgumentParser


def to_rgb565(r, g, b):
    return ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)


def from_rgb565(value):
    r = (value >> 8) & 0xF8
    g = (value >> 3) & 0xFC
    b = (value << 3) & 0xF8
    return r, g, b


def cmd_encode(args):
    img = Image.open(args.input)
    if img.mode in ("RGBA", "LA", "P"):
        rgba = img.convert("RGBA")
        background = Image.new("RGB", img.size, (255, 255, 255))
        background.paste(rgba, mask=rgba.split()[3])
        img = background
    else:
        img = img.convert("RGB")
    scaled_width = round(img.width * args.scale / 100)
    scaled_height = round(img.height * args.scale / 100)

    if scaled_width != img.width or scaled_height != img.height:
        print(f"Resizing image from {img.width}x{img.height} to {scaled_width}x{scaled_height}")
        img = img.resize((scaled_width, scaled_height), Image.LANCZOS)

    width, height = img.width, img.height
    pixels = list(img.get_flattened_data())
    values = [to_rgb565(r, g, b) for r, g, b in pixels]

    dest_filename = args.output.split("/")[-1].split(".")[0]
    array_name = dest_filename.lower().replace(" ", "_")

    lines = [
        "#pragma once",
        "#include <stdint.h>",
        "",
        f"// Auto-generated from {args.input} -- {width}x{height} RGB565",
        f"// {width*height} pixels, {width*height*2} bytes stored in flash",
        "",
        f"static const uint16_t {array_name}_width = {width};",
        f"static const uint16_t {array_name}_height = {height};",
        "",
        f"static const uint16_t {array_name}[{width*height}] = {{",
    ]

    row_size = 16
    for i in range(0, len(values), row_size):
        chunk = values[i:i+row_size]
        lines.append("    " + ", ".join(f"0x{v:04X}" for v in chunk) + ",")

    lines += ["};", ""]

    with open(args.output, "w") as f:
        f.write("\n".join(lines))

    print(f"OK: {len(values)} pixels, {len(values)*2} bytes -> {args.output}")


def cmd_decode(args):
    with open(args.input, "r") as f:
        text = f.read()

    width_match = re.search(r"_width\s*=\s*(\d+)", text)
    height_match = re.search(r"_height\s*=\s*(\d+)", text)
    if not width_match or not height_match:
        raise ValueError("Could not find width/height declarations in header file")

    width = int(width_match.group(1))
    height = int(height_match.group(1))

    hex_values = re.findall(r"0x([0-9A-Fa-f]{4})", text)
    values = [int(v, 16) for v in hex_values]

    expected = width * height
    if len(values) != expected:
        raise ValueError(f"Expected {expected} pixel values, found {len(values)}")

    pixels = [from_rgb565(v) for v in values]
    img = Image.new("RGB", (width, height))
    img.putdata(pixels)
    img.save(args.output)

    print(f"OK: {width}x{height} image -> {args.output}")


parser = ArgumentParser(description="Convert images to/from RGB565 C header files")
subparsers = parser.add_subparsers(dest="command", required=True)

encode_parser = subparsers.add_parser("encode", help="Convert a PNG to an RGB565 C header file")
encode_parser.add_argument("input", help="Path to the input image (e.g. src/img/background.png)")
encode_parser.add_argument("output", help="Path to the output header file (e.g. src/img/background.h)")
encode_parser.add_argument("--scale", type=float, default=100, help="Percentage to scale the image to (default: 100)")

decode_parser = subparsers.add_parser("decode", help="Convert an RGB565 C header file back to a PNG")
decode_parser.add_argument("input", help="Path to the input header file (e.g. src/img/background.h)")
decode_parser.add_argument("output", help="Path to the output image (e.g. src/img/background.png)")

args = parser.parse_args()

if args.command == "encode":
    cmd_encode(args)
elif args.command == "decode":
    cmd_decode(args)
