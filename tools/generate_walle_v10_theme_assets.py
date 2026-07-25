#!/usr/bin/env python3
"""Build the WALL-E EARTH static layer and dynamic leaf assets.

The approved WALL-E EARTH composition is deliberately kept as a raster
background: the city, WALL-E illustration, panels, labels, cassette, sprout,
and texture therefore remain pixel-identical to the design. Only live values
are removed and re-rendered by LVGL. The ten quota leaves are exported as
palette-matched active/inactive variants so their state can still follow live
quota data.
"""

from __future__ import annotations

import argparse
from collections import deque
from pathlib import Path

from generate_walle_theme_assets import (
    decode_png,
    encode_png,
    inpaint_region,
    make_palette_mapper,
    pixel,
    rgb565le,
    set_pixel,
)


WIDTH = 480
HEIGHT = 480
LEAF_X = 21
LEAF_Y = 296
LEAF_W = 251
LEAF_H = 46
LEAF_SLOT_X = (21, 47, 72, 97, 122, 147, 172, 197, 222, 247)


def move_masked_region(
    source: bytearray,
    output: bytearray,
    bounds: tuple[int, int, int, int],
    predicate,
    dx: int,
    dy: int,
    dilation: int = 1,
) -> None:
    """Move a textured glyph group while preserving its antialiased edge."""
    x0, y0, x1, y1 = bounds
    mask = {
        (x, y)
        for y in range(y0, y1)
        for x in range(x0, x1)
        if predicate(pixel(source, x, y))
    }
    for _ in range(dilation):
        expanded = set(mask)
        for x, y in mask:
            for offset_x, offset_y in (
                (-1, 0),
                (1, 0),
                (0, -1),
                (0, 1),
            ):
                neighbor_x = x + offset_x
                neighbor_y = y + offset_y
                if (
                    x0 <= neighbor_x < x1
                    and y0 <= neighbor_y < y1
                ):
                    expanded.add((neighbor_x, neighbor_y))
        mask = expanded

    samples = {
        point: pixel(source, *point)
        for point in mask
    }
    inpaint_region(
        source, output, bounds, predicate, dilation)
    for (x, y), color in samples.items():
        destination_x = x + dx
        destination_y = y + dy
        if (
            0 <= destination_x < WIDTH
            and 0 <= destination_y < HEIGHT
        ):
            set_pixel(
                output, destination_x, destination_y, color)


def largest_component(
    source: bytearray,
    bounds: tuple[int, int, int, int],
    predicate,
) -> set[tuple[int, int]]:
    x0, y0, x1, y1 = bounds
    candidates = {
        (x, y)
        for y in range(y0, y1)
        for x in range(x0, x1)
        if predicate(pixel(source, x, y))
    }
    largest: set[tuple[int, int]] = set()
    while candidates:
        seed = candidates.pop()
        component = {seed}
        queue = deque([seed])
        while queue:
            x, y = queue.popleft()
            for dx in (-1, 0, 1):
                for dy in (-1, 0, 1):
                    if dx == 0 and dy == 0:
                        continue
                    neighbor = (x + dx, y + dy)
                    if neighbor in candidates:
                        candidates.remove(neighbor)
                        component.add(neighbor)
                        queue.append(neighbor)
        if len(component) > len(largest):
            largest = component
    return largest


def crop_pixel(
    source: bytearray, crop_index: int
) -> tuple[int, int, int]:
    x = LEAF_X + crop_index % LEAF_W
    y = LEAF_Y + crop_index // LEAF_W
    return pixel(source, x, y)


def set_crop_pixel(
    output: bytearray,
    crop_index: int,
    color: tuple[int, int, int],
) -> None:
    offset = crop_index * 3
    output[offset : offset + 3] = bytes(color)


def make_stretched_palette_mapper(
    source_colors: list[tuple[int, int, int]],
    target_colors: list[tuple[int, int, int]],
):
    """Map the full source contrast range onto the target palette."""
    luma = lambda color: (
        54 * color[0] + 183 * color[1] + 19 * color[2]
    ) // 256
    source_levels = sorted(luma(color) for color in source_colors)
    target_levels = sorted(luma(color) for color in target_colors)
    source_low = source_levels[len(source_levels) // 20]
    source_high = source_levels[len(source_levels) * 19 // 20]
    target_low = target_levels[len(target_levels) // 20]
    target_high = target_levels[len(target_levels) * 19 // 20]
    target_mapper = make_palette_mapper(target_colors)

    def mapped(
        color: tuple[int, int, int],
    ) -> tuple[int, int, int]:
        level = luma(color)
        ratio = (
            max(source_low, min(source_high, level)) - source_low
        ) / max(1, source_high - source_low)
        target_level = round(
            target_low + ratio * (target_high - target_low))
        return target_mapper(
            (target_level, target_level, target_level))

    return mapped


def equalize_leaf_luma(
    buffer: bytearray,
    mask: bytearray,
    leaf_index: int,
    target_luma: float,
) -> None:
    indices = [
        index
        for index, value in enumerate(mask)
        if value == leaf_index
    ]
    if not indices:
        return

    def luma_at(index: int) -> int:
        offset = index * 3
        red, green, blue = buffer[offset : offset + 3]
        return (54 * red + 183 * green + 19 * blue) // 256

    current_luma = (
        sum(luma_at(index) for index in indices) / len(indices)
    )
    scale = target_luma / max(1.0, current_luma)
    for index in indices:
        offset = index * 3
        for channel in range(3):
            buffer[offset + channel] = min(
                255, round(buffer[offset + channel] * scale))


def build_leaf_assets(
    source: bytearray,
) -> tuple[bytes, bytes, bytes]:
    green = lambda color: (
        color[1] > 35
        and color[1] > color[0] * 1.15
        and color[1] > color[2] * 1.15
    )
    gray = lambda color: (
        (54 * color[0] + 183 * color[1] + 19 * color[2]) // 256 > 18
    )

    mask = bytearray(LEAF_W * LEAF_H)
    active_colors: list[tuple[int, int, int]] = []
    inactive_colors: list[tuple[int, int, int]] = []
    for index, slot_x in enumerate(LEAF_SLOT_X, start=1):
        predicate = green if index <= 8 else gray
        component = largest_component(
            source, (slot_x, 297, slot_x + 25, 341), predicate
        )
        if len(component) < 300:
            raise RuntimeError(
                f"leaf {index} mask unexpectedly small: {len(component)}"
            )
        for x, y in component:
            crop_index = (y - LEAF_Y) * LEAF_W + (x - LEAF_X)
            mask[crop_index] = index
            color = pixel(source, x, y)
            if index <= 8:
                active_colors.append(color)
            else:
                inactive_colors.append(color)

    to_active = make_stretched_palette_mapper(
        inactive_colors, active_colors)
    to_inactive = make_stretched_palette_mapper(
        active_colors, inactive_colors)
    active = bytearray(LEAF_W * LEAF_H * 3)
    inactive = bytearray(LEAF_W * LEAF_H * 3)
    for crop_index, leaf_index in enumerate(mask):
        color = crop_pixel(source, crop_index)
        active_color = color
        inactive_color = color
        if leaf_index:
            if leaf_index <= 8:
                inactive_color = to_inactive(color)
            else:
                active_color = to_active(color)
        set_crop_pixel(active, crop_index, active_color)
        set_crop_pixel(inactive, crop_index, inactive_color)

    luma = lambda color: (
        54 * color[0] + 183 * color[1] + 19 * color[2]
    ) // 256
    active_target = sum(
        luma(color) for color in active_colors) / len(active_colors)
    inactive_target = sum(
        luma(color) for color in inactive_colors) / len(inactive_colors)
    for leaf_index in range(1, len(LEAF_SLOT_X) + 1):
        equalize_leaf_luma(
            active, mask, leaf_index, active_target)
        equalize_leaf_luma(
            inactive, mask, leaf_index, inactive_target)
    return rgb565le(active), rgb565le(inactive), bytes(mask)


def build_assets(root: Path) -> None:
    reference_path = root / "docs/assets/walle-theme-v10.png"
    clean_path = root / "docs/assets/walle-theme-v10-clean.png"
    theme_dir = root / "firmware/data/themes"

    source = decode_png(reference_path)
    clean = bytearray(source)

    cream = lambda color: (
        color[0] > 145
        and color[1] > 105
        and color[2] > 55
        and color[0] >= color[1] >= color[2]
    )
    green = lambda color: (
        color[1] > 60
        and color[1] > color[0] * 1.12
        and color[1] > color[2] * 1.15
    )

    title_cream = lambda color: (
        color[0] > 170
        and color[1] > 120
        and color[2] > 65
        and color[0] >= color[1] >= color[2]
    )
    move_masked_region(
        source, clean, (16, 10, 242, 42),
        title_cream, 0, 4, 1)
    move_masked_region(
        source, clean, (340, 294, 405, 317),
        cream, 1, 0, 1)

    regions = (
        ((408, 15, 466, 46), cream, 2),   # battery percentage
        ((31, 122, 205, 154), green, 2),  # primary heading
        ((24, 151, 255, 261), cream, 3),  # primary value
        ((344, 129, 454, 164), green, 2),  # secondary heading
        ((321, 164, 461, 261), cream, 3),  # secondary value
        ((144, 339, 277, 413), cream, 3),  # quota value
        ((298, 367, 453, 413), cream, 3),  # reset values
        ((47, 430, 71, 467), cream, 2),  # active task count
    )
    for bounds, predicate, dilation in regions:
        inpaint_region(source, clean, bounds, predicate, dilation)

    active, inactive, mask = build_leaf_assets(source)
    clean_path.write_bytes(encode_png(clean))
    theme_dir.mkdir(parents=True, exist_ok=True)
    (theme_dir / "walle_v10_bg.rgb565").write_bytes(rgb565le(clean))
    (theme_dir / "walle_v10_leaves_active.rgb565").write_bytes(active)
    (theme_dir / "walle_v10_leaves_inactive.rgb565").write_bytes(inactive)
    (theme_dir / "walle_v10_leaves_mask.bin").write_bytes(mask)


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
