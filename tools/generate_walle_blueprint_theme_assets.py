#!/usr/bin/env python3
"""Build the WALL-E BLUEPRINT static layer and dynamic cell assets.

The approved blueprint is preserved as a full-screen RGB565 background. Live
headings and values are removed from that raster and rendered by LVGL. The
visible navy paper is reconstructed as one continuous field so cleared text
areas cannot leave rectangular color patches. Ten quota cells and seven
battery cells retain the source palette through active, inactive, and per-cell
mask assets.
"""

from __future__ import annotations

import argparse
from collections import deque
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


def is_navy(color: tuple[int, int, int]) -> bool:
    return (
        color[0] < 38
        and color[1] < 82
        and color[2] < 135
        and color[2] > color[1] * 1.08
    )


def inside_any(
    x: int,
    y: int,
    bounds_list: tuple[tuple[int, int, int, int], ...],
) -> bool:
    return any(
        x0 <= x < x1 and y0 <= y < y1
        for x0, y0, x1, y1 in bounds_list
    )


def fill_navy_gradient(
    source: bytearray,
    output: bytearray,
    bounds: tuple[int, int, int, int],
) -> None:
    """Reconstruct a live-text rectangle from nearby navy-only samples."""
    x0, y0, x1, y1 = bounds

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


def build_background_mask(clean: bytearray) -> bytearray:
    """Find paper pixels without crossing the preserved line artwork."""
    mask = bytearray(WIDTH * HEIGHT)
    queue: deque[int] = deque()

    def add_seed(x: int, y: int) -> None:
        index = y * WIDTH + x
        if mask[index] or not is_navy(pixel(clean, x, y)):
            return
        mask[index] = 1
        queue.append(index)

    for x in range(WIDTH):
        add_seed(x, 0)
        add_seed(x, HEIGHT - 1)
    for y in range(HEIGHT):
        add_seed(0, y)
        add_seed(WIDTH - 1, y)

    # These navy fields are enclosed by their gold outlines, but still form
    # background paper and must be repainted together with the open page.
    for x, y in ((330, 20), (20, 400), (360, 350)):
        add_seed(x, y)

    while queue:
        index = queue.popleft()
        x = index % WIDTH
        y = index // WIDTH
        if x > 0:
            add_seed(x - 1, y)
        if x + 1 < WIDTH:
            add_seed(x + 1, y)
        if y > 0:
            add_seed(x, y - 1)
        if y + 1 < HEIGHT:
            add_seed(x, y + 1)
    return mask


def repaint_navy_paper(
    source: bytearray,
    clean: bytearray,
    excluded_samples: tuple[tuple[int, int, int, int], ...],
) -> None:
    """Redraw one continuous navy paper field behind preserved line art."""
    background = build_background_mask(clean)
    step = 24
    grid_x = list(range(0, WIDTH, step))
    grid_y = list(range(0, HEIGHT, step))
    if grid_x[-1] != WIDTH - 1:
        grid_x.append(WIDTH - 1)
    if grid_y[-1] != HEIGHT - 1:
        grid_y.append(HEIGHT - 1)

    grid: list[list[tuple[int, int, int]]] = []
    for center_y in grid_y:
        row: list[tuple[int, int, int]] = []
        for center_x in grid_x:
            samples: list[tuple[int, int, int]] = []
            for radius in (18, 36, 60):
                samples.clear()
                for y in range(
                    max(0, center_y - radius),
                    min(HEIGHT, center_y + radius + 1),
                    2,
                ):
                    for x in range(
                        max(0, center_x - radius),
                        min(WIDTH, center_x + radius + 1),
                        2,
                    ):
                        if not background[y * WIDTH + x]:
                            continue
                        if inside_any(x, y, excluded_samples):
                            continue
                        color = pixel(source, x, y)
                        if is_navy(color):
                            samples.append(color)
                if len(samples) >= 24:
                    break
            if samples:
                samples.sort(key=lambda color: sum(color))
                trim = len(samples) // 8
                kept = samples[trim : len(samples) - trim] if trim else samples
                count = len(kept)
                row.append(tuple(
                    sum(color[channel] for color in kept) // count
                    for channel in range(3)
                ))
            else:
                row.append((0, 39, 83))
        grid.append(row)

    for y in range(HEIGHT):
        gy = min(y // step, len(grid_y) - 2)
        y0 = grid_y[gy]
        y1 = grid_y[gy + 1]
        wy = 0 if y1 == y0 else (y - y0) / (y1 - y0)
        for x in range(WIDTH):
            if not background[y * WIDTH + x]:
                continue
            gx = min(x // step, len(grid_x) - 2)
            x0 = grid_x[gx]
            x1 = grid_x[gx + 1]
            wx = 0 if x1 == x0 else (x - x0) / (x1 - x0)
            top = tuple(
                grid[gy][gx][channel] * (1 - wx)
                + grid[gy][gx + 1][channel] * wx
                for channel in range(3)
            )
            bottom = tuple(
                grid[gy + 1][gx][channel] * (1 - wx)
                + grid[gy + 1][gx + 1][channel] * wx
                for channel in range(3)
            )
            # A small deterministic correlated dither prevents RGB565 bands
            # without recreating the rectangular texture mismatch.
            noise = ((x * 17 + y * 31 + x * y * 3) & 3) - 1
            color = tuple(
                max(0, min(255, round(
                    top[channel] * (1 - wy)
                    + bottom[channel] * wy
                    + noise
                )))
                for channel in range(3)
            )
            set_pixel(clean, x, y, color)  # type: ignore[arg-type]


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

    # First remove live copy without allowing the warm line art or generated
    # print shadow to bleed into the clean layer. A continuous paper pass below
    # then removes the remaining per-rectangle color discontinuities.
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

    repaint_navy_paper(
        source,
        clean,
        live_regions
        + (
            (326, 13, 395, 32),
            (16, 390, 229, 416),
            (5, 429, 464, 472),
            (349, 115, 433, 120),
        ),
    )

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
