#!/usr/bin/env python3
"""Build the Three-Body impasto Starry Night runtime background.

The approved 480x480 oil-painted composition remains the source of truth for
the Solar System, Pluto canyon, panel chrome, fixed headings, and texture.
Only fields that change at runtime are removed and inpainted before the image
is converted to little-endian RGB565 for the device filesystem.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from generate_walle_theme_assets import (
    decode_png,
    encode_png,
    fill_horizontal_gradient,
    inpaint_region,
    luma,
    rgb565le,
)


def build_assets(root: Path) -> None:
    reference_path = (
        root
        / "docs/assets/three-body-starry-night-oil-v1.png"
    )
    clean_path = root / "docs/assets/three-body-starry-night-runtime-clean.png"
    theme_path = root / "firmware/data/themes/three_body_bg.rgb565"

    source = decode_png(reference_path)
    clean = bytearray(source)

    # All dynamic glyphs are bright cream/yellow on very dark blue panels.
    # Restrict the predicate to their tight bounds so nearby borders, icons,
    # fixed headings, and the small painted underline strokes stay untouched.
    dynamic_glyph = lambda color: (
        luma(color) >= 105 and max(color) >= 145
    )
    regions = (
        (20, 367, 113, 409),   # today value + unit
        (137, 367, 229, 409),  # seven-day value + unit
        (257, 367, 345, 409),  # seven-day quota + percent
        (376, 367, 461, 409),  # battery value + percent
        (88, 438, 153, 464),   # reset time
        (192, 432, 259, 465),  # task count + label
        (345, 438, 380, 464),  # sync state
    )
    for bounds in regions:
        inpaint_region(source, clean, bounds, dynamic_glyph, 2)

    # V14 swaps the fourth card and the first footer capsule: RESET moves into
    # the card while battery status moves below. Remove the two old fixed
    # captions/icons without touching the cyan panel borders.
    neutral_caption = lambda color: (
        luma(color) >= 65 and max(color) >= 95
        and max(color) - min(color) <= 70
    )
    inpaint_region(
        source, clean, (388, 348, 445, 366), neutral_caption, 2
    )

    # V15 removes the four decorative status lamps and replaces the uneven
    # footer artwork with three equal runtime capsules. Rebuild the complete
    # footer strip from the untouched edge texture so no old frame, icon, text,
    # separator, or lamp can show through the LVGL layer.
    fill_horizontal_gradient(source, clean, (7, 425, 473, 469), 4, 476)

    clean_path.write_bytes(encode_png(clean))
    theme_path.parent.mkdir(parents=True, exist_ok=True)
    theme_path.write_bytes(rgb565le(clean))


def main() -> int:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--root",
        type=Path,
        default=Path(__file__).resolve().parents[1],
    )
    args = parser.parse_args()
    build_assets(args.root.resolve())
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
