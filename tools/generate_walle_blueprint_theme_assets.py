#!/usr/bin/env python3
"""Build the WALL-E BLUEPRINT static layer and quota-bar assets.

The approved blueprint is preserved as a full-screen RGB565 background. Live
headings and values are removed from that raster and rendered by LVGL. Ten
quota cells and seven battery cells retain the source palette through active,
inactive, and per-cell mask assets.
"""

from __future__ import annotations

import argparse
from pathlib import Path

from generate_walle_theme_assets import (
    decode_png,
    encode_png,
    pixel,
    rgb565le,
    set_pixel,
)


WIDTH = 480
HEIGHT = 480
BAR_X = 14
BAR_Y = 391
BAR_W = 216
BAR_H = 25
BAR_SLOTS = tuple(
    (19 + index * 21, 37 + index * 21)
    for index in range(10)
)
BAR_CELL_Y0 = 392
BAR_CELL_Y1 = 414

ACTIVE_BAR_ROWS = (
    (240, 166, 5),
    (240, 166, 5),
    *((235, 166, 14),) * 17,
    (241, 171, 12),
    (108, 104, 84),
    (28, 24, 5),
)
INACTIVE_BAR_ROWS = (
    (89, 115, 148),
    (89, 115, 148),
    *((90, 117, 147),) * 17,
    (93, 118, 147),
    (52, 79, 116),
    (1, 28, 65),
)

BATTERY_X = 326
BATTERY_Y = 13
BATTERY_W = 69
BATTERY_H = 19
BATTERY_SOURCE_SLOTS = tuple(
    (326 + index * 10, 335 + index * 10)
    for index in range(6)
)
BATTERY_SLOTS = tuple(
    (326 + index * 10, 335 + index * 10)
    for index in range(7)
)


def fill_navy_gradient(
    source: bytearray,
    output: bytearray,
    bounds: tuple[int, int, int, int],
) -> None:
    """Reconstruct a live-text rectangle from nearby navy-only samples."""
    x0, y0, x1, y1 = bounds

    def is_navy(color: tuple[int, int, int]) -> bool:
        return (
            color[0] < 38
            and color[1] < 82
            and color[2] < 135
            and color[2] > color[1] * 1.08
        )

    def average(colors: list[tuple[int, int, int]]):
        if not colors:
            return None
        count = len(colors)
        return tuple(
            sum(color[channel] for color in colors) // count
            for channel in range(3)
        )

    previous_left: tuple[int, int, int] | None = None
    previous_right: tuple[int, int, int] | None = None
    for y in range(y0, y1):
        left_samples = [
            pixel(source, x, sample_y)
            for sample_y in range(max(0, y - 4), min(HEIGHT, y + 5))
            for x in range(max(0, x0 - 24), x0)
            if is_navy(pixel(source, x, sample_y))
        ]
        right_samples = [
            pixel(source, x, sample_y)
            for sample_y in range(max(0, y - 4), min(HEIGHT, y + 5))
            for x in range(x1, min(WIDTH, x1 + 24))
            if is_navy(pixel(source, x, sample_y))
        ]
        left = average(left_samples) or previous_left
        right = average(right_samples) or previous_right
        if left is None and right is None:
            left = right = (0, 39, 83)
        elif left is None:
            left = right
        elif right is None:
            right = left
        assert left is not None and right is not None
        previous_left = left
        previous_right = right

        denominator = max(1, x1 - x0 - 1)
        for x in range(x0, x1):
            weight = x - x0
            color = tuple(
                (
                    left[channel] * (denominator - weight)
                    + right[channel] * weight
                )
                // denominator
                for channel in range(3)
            )
            set_pixel(output, x, y, color)  # type: ignore[arg-type]


def build_bar_assets(
    source: bytearray,
) -> tuple[bytes, bytes, bytes]:
    del source
    mask = bytearray(BAR_W * BAR_H)
    active = bytearray(BAR_W * BAR_H * 3)
    inactive = bytearray(BAR_W * BAR_H * 3)

    for slot_index, (x0, x1) in enumerate(BAR_SLOTS, start=1):
        for y in range(BAR_CELL_Y0, BAR_CELL_Y1):
            for x in range(x0, x1):
                crop_index = (y - BAR_Y) * BAR_W + (x - BAR_X)
                mask[crop_index] = slot_index
                offset = crop_index * 3
                row = y - BAR_CELL_Y0
                active[offset : offset + 3] = bytes(ACTIVE_BAR_ROWS[row])
                inactive[offset : offset + 3] = bytes(INACTIVE_BAR_ROWS[row])

    return rgb565le(active), rgb565le(inactive), bytes(mask)


def build_battery_assets(
    source: bytearray,
) -> tuple[bytes, bytes, bytes]:
    pixels = BATTERY_W * BATTERY_H
    active = bytearray(pixels * 3)
    inactive = bytearray(pixels * 3)
    mask = bytearray(pixels)
    inactive_x0, inactive_x1 = BATTERY_SLOTS[-1]

    for slot_index, (x0, x1) in enumerate(BATTERY_SLOTS, start=1):
        source_x0, source_x1 = BATTERY_SOURCE_SLOTS[
            (slot_index - 1) % len(BATTERY_SOURCE_SLOTS)
        ]
        target_width = x1 - x0
        for y in range(BATTERY_Y, BATTERY_Y + BATTERY_H):
            for x in range(x0, x1):
                relative_x = x - x0
                source_x = source_x0 + min(
                    source_x1 - source_x0 - 1,
                    relative_x * (source_x1 - source_x0) // target_width,
                )
                empty_x = inactive_x0 + min(
                    inactive_x1 - inactive_x0 - 1,
                    relative_x * (inactive_x1 - inactive_x0) // target_width,
                )
                crop_index = (
                    (y - BATTERY_Y) * BATTERY_W + (x - BATTERY_X)
                )
                mask[crop_index] = slot_index
                offset = crop_index * 3
                active[offset : offset + 3] = bytes(
                    pixel(source, source_x, y)
                )
                inactive[offset : offset + 3] = bytes(
                    pixel(source, empty_x, y)
                )

    return rgb565le(active), rgb565le(inactive), bytes(mask)


def build_assets(root: Path) -> None:
    reference_path = (
        root / "docs/assets/walle-theme-v15-refined-v4.png"
    )
    clean_path = (
        root / "docs/assets/walle-theme-blueprint-clean.png"
    )
    theme_dir = root / "firmware/data/themes"

    source = decode_png(reference_path)
    clean = bytearray(source)

    # These rectangles contain only live copy over an otherwise empty navy
    # field. Reconstruct each row from nearby navy-only samples so neither the
    # warm line art nor the generated print shadow bleeds into the clean layer.
    live_regions = (
        (405, 7, 465, 40),    # battery percentage
        (15, 66, 119, 85),    # primary heading
        (14, 94, 149, 115),   # primary numeric value above leader
        (14, 115, 183, 181),  # complete primary value below leader
        (357, 81, 425, 106),  # secondary heading
        (344, 120, 440, 176), # secondary value
        (14, 318, 115, 380),  # quota percentage
        (349, 353, 460, 401), # reset value
        (40, 435, 198, 465),  # task text
    )
    for bounds in live_regions:
        fill_navy_gradient(source, clean, bounds)

    # The three footer capsules, lamps, and labels are rendered by LVGL so
    # their widths and typography can share one layout system. Remove the
    # source artwork here to avoid doubled borders and glyphs.
    for bounds in (
        (5, 429, 218, 472),
        (217, 429, 230, 472),
        (229, 429, 324, 472),
        (323, 429, 333, 472),
        (332, 429, 464, 472),
    ):
        fill_navy_gradient(source, clean, bounds)

    # Battery cells and quota cells are dynamic. Preserve their outer frames,
    # but clear the source fills and old fourteen-cell divider pattern.
    fill_navy_gradient(source, clean, (326, 13, 395, 32))
    fill_navy_gradient(source, clean, (16, 390, 229, 416))

    # Raise the 7 DAYS divider by five pixels to give the value group more room.
    fill_navy_gradient(source, clean, (349, 115, 433, 120))
    for x in range(351, 431):
        set_pixel(clean, x, 112, pixel(source, x, 117))

    active, inactive, mask = build_bar_assets(source)
    battery_active, battery_inactive, battery_mask = (
        build_battery_assets(source)
    )
    clean_path.write_bytes(encode_png(clean))
    theme_dir.mkdir(parents=True, exist_ok=True)
    (theme_dir / "walle_blueprint_bg.rgb565").write_bytes(
        rgb565le(clean)
    )
    (theme_dir / "walle_blueprint_bar_active.rgb565").write_bytes(
        active
    )
    (theme_dir / "walle_blueprint_bar_inactive.rgb565").write_bytes(
        inactive
    )
    (theme_dir / "walle_blueprint_bar_mask.bin").write_bytes(mask)
    (theme_dir / "walle_blueprint_battery_active.rgb565").write_bytes(
        battery_active
    )
    (theme_dir / "walle_blueprint_battery_inactive.rgb565").write_bytes(
        battery_inactive
    )
    (theme_dir / "walle_blueprint_battery_mask.bin").write_bytes(
        battery_mask
    )


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
