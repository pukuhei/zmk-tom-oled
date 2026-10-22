#!/usr/bin/env python3
"""Compile the AI quota renderer for the host and compare it with the approved frames.

The reference PNGs in assets/agent_quota are layout "B" from the LiTom 128x32 concept.
Requires a C compiler (cc) and Pillow.
"""

import subprocess
import sys
import tempfile
from pathlib import Path

from PIL import Image


ROOT = Path(__file__).resolve().parents[1]
WIDGETS = ROOT / "boards/shields/tom_oled/widgets"
ASSETS = ROOT / "assets/agent_quota"

# source, primary %, secondary %, primary window min, secondary window min, reset min
UNKNOWN = 255
CASES = {
    "normal": ("CODEX", 72, 41, 300, 10080, 134),
    "low": ("CODEX", 8, 41, 300, 10080, 134),
    "zero": ("CODEX", 0, 41, 300, 10080, 134),
    "nodata": ("CODEX", UNKNOWN, UNKNOWN, 300, 10080, -1),
    "claude": ("CLAUDE", 72, 41, 300, 10080, 134),
}

HOST_MAIN = r"""
#include <stdio.h>
#include <stdlib.h>
#include "agent_quota_render.h"
int main(int argc, char **argv) {
    if (argc != 7) return 2;
    struct tom_oled_quota_render_input in = {
        argv[1], (uint8_t)atoi(argv[2]), (uint8_t)atoi(argv[3]),
        (uint16_t)atoi(argv[4]), (uint16_t)atoi(argv[5]), atoi(argv[6]),
    };
    uint8_t frame[TOM_OLED_QUOTA_FRAME_BYTES];
    tom_oled_quota_render(frame, &in);
    fwrite(frame, 1, sizeof(frame), stdout);
    return 0;
}
"""


def main() -> int:
    with tempfile.TemporaryDirectory() as tmp:
        main_c = Path(tmp) / "main.c"
        binary = Path(tmp) / "quota"
        main_c.write_text(HOST_MAIN)
        subprocess.run(["cc", "-std=c99", "-Wall", "-Wextra", "-Werror", f"-I{WIDGETS}",
                        str(main_c), str(WIDGETS / "agent_quota_render.c"), "-o", str(binary)],
                       check=True)
        failed = False
        for name, args in CASES.items():
            raw = subprocess.run([str(binary), *map(str, args)], check=True,
                                 capture_output=True).stdout
            actual = Image.frombytes("1", (128, 32), raw)
            expected = Image.open(ASSETS / f"quota_{name}.png").convert("1")
            diff = sum(bool(a) != bool(e) for a, e in zip(actual.getdata(), expected.getdata()))
            print(f"{name}: {diff} differing pixels")
            failed |= diff != 0
    return 1 if failed else 0


if __name__ == "__main__":
    sys.exit(main())
