#!/usr/bin/env python3
"""
Generates a table of unsigned 1.15-bit fractions representing one quarter cycle of the PDLC
output waveform prior to scaling.
"""

import argparse
import sys
import math

QUARTER_WAVE_SAMPLES = 128
AMPLITUDE = 32768

def gen_entry(index):
    return int(round(math.sin(0.5 * math.pi * index / QUARTER_WAVE_SAMPLES) * AMPLITUDE))


if __name__ == '__main__':
    parser = argparse.ArgumentParser(
        description=__doc__,
        formatter_class=argparse.RawDescriptionHelpFormatter,
        allow_abbrev=False,
    )

    parser.add_argument("-o", "--output", required=True, help="Output file")

    args = parser.parse_args()

    with open(args.output, 'w') as out:
        out.write("#include <stdint.h>\n")
        out.write(f"#define PDLC_QUARTER_WAVE_SAMPLES ({QUARTER_WAVE_SAMPLES})\n")
        out.write("static const uint16_t pdlc_quarter_wave_table[PDLC_QUARTER_WAVE_SAMPLES + 1] = {\n")
        for index in range(0, QUARTER_WAVE_SAMPLES + 1):
            out.write(f"  {gen_entry(index)},\n")
        out.write("};\n")

    sys.exit(0)
