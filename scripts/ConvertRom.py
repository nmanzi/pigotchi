# convert_rom.py
# Usage: python3 convert_rom.py input.rom output.h

import sys
from argparse import ArgumentParser

parser = ArgumentParser(description='Convert a ROM binary file into a C header file with a u12_t array.')
parser.add_argument('input', help='Input ROM binary file')
parser.add_argument('output', help='Output C header file')
args = parser.parse_args()

with open(args.input, 'rb') as f:
    data = f.read()

# The P1 ROM is stored as pairs of bytes encoding one 12-bit opcode:
# byte[n*2]   = high nibble (bits 11:8) in low 4 bits
# byte[n*2+1] = low byte (bits 7:0)
words = []
for i in range(0, len(data) - 1, 2):
    word = ((data[i] & 0x0F) << 8) | data[i + 1]
    words.append(word)

lines = [
    '#pragma once',
    '#include "../lib/hal_types.h"',
    '',
    '#ifdef __cplusplus',
    'extern "C" {',
    '#endif',
    '',
    f'static const u12_t tamagotchi_rom[{len(words)}] = {{',
]

for i in range(0, len(words), 8):
    chunk = words[i:i+8]
    lines.append('    ' + ', '.join(f'0x{w:03X}' for w in chunk) + ',')

lines.append('};')
lines.append('')
lines.append('#ifdef __cplusplus')
lines.append('}')
lines.append('#endif')

with open(args.output, 'w') as f:
    f.write('\n'.join(lines))

print(f'Header file written to {args.output}')