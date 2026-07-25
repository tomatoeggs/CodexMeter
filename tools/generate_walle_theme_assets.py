#!/usr/bin/env python3
"""Build the WALL-E V9 static layer and dynamic leaf assets.

The generator starts from the approved raster reference, removes only pixels
that belong to live text/status glyphs, and diffuses the surrounding texture
through those small masks. This avoids the visible rectangular bands produced
by clearing whole value bounding boxes.
"""

from __future__ import annotations

import argparse
import bisect
import shutil
import struct
import subprocess
import zlib
from pathlib import Path


WIDTH = 480
HEIGHT = 480
LEAF_X = 23
LEAF_Y = 259
LEAF_W = 219
LEAF_H = 93
PNG_SIGNATURE = b"\x89PNG\r\n\x1a\n"


def decode_png(path: Path) -> bytearray:
    ffmpeg = shutil.which("ffmpeg")
    if not ffmpeg:
        raise RuntimeError("ffmpeg is required to decode the reference PNG")
    result = subprocess.run(
        [
            ffmpeg,
            "-loglevel",
            "error",
            "-i",
            str(path),
            "-f",
            "rawvideo",
            "-pix_fmt",
            "rgb24",
            "-",
        ],
        check=True,
        stdout=subprocess.PIPE,
    )
    expected = WIDTH * HEIGHT * 3
    if len(result.stdout) != expected:
        raise RuntimeError(
            f"decoded {len(result.stdout)} bytes; expected {expected}"
        )
    return bytearray(result.stdout)


def pixel(buffer: bytearray, x: int, y: int) -> tuple[int, int, int]:
    offset = (y * WIDTH + x) * 3
    return tuple(buffer[offset : offset + 3])  # type: ignore[return-value]


def set_pixel(
    buffer: bytearray, x: int, y: int, color: tuple[int, int, int]
) -> None:
    offset = (y * WIDTH + x) * 3
    buffer[offset : offset + 3] = bytes(color)


def inpaint_region(
    source: bytearray,
    output: bytearray,
    bounds: tuple[int, int, int, int],
    predicate,
    dilation: int = 1,
) -> None:
    x0, y0, x1, y1 = bounds
    masked: set[tuple[int, int]] = set()
    for y in range(y0, y1):
        for x in range(x0, x1):
            if predicate(pixel(source, x, y)):
                masked.add((x, y))

    for _ in range(dilation):
        expanded = set(masked)
        for x, y in masked:
            for dx, dy in ((-1, 0), (1, 0), (0, -1), (0, 1)):
                nx, ny = x + dx, y + dy
                if x0 <= nx < x1 and y0 <= ny < y1:
                    expanded.add((nx, ny))
        masked = expanded

    unresolved = set(masked)
    neighbors = (
        (-1, -1),
        (0, -1),
        (1, -1),
        (-1, 0),
        (1, 0),
        (-1, 1),
        (0, 1),
        (1, 1),
    )
    while unresolved:
        batch: list[tuple[int, int, tuple[int, int, int]]] = []
        for x, y in unresolved:
            colors = []
            for dx, dy in neighbors:
                nx, ny = x + dx, y + dy
                if not (0 <= nx < WIDTH and 0 <= ny < HEIGHT):
                    continue
                if (nx, ny) in unresolved:
                    continue
                colors.append(pixel(output, nx, ny))
            if colors:
                count = len(colors)
                batch.append(
                    (
                        x,
                        y,
                        (
                            sum(color[0] for color in colors) // count,
                            sum(color[1] for color in colors) // count,
                            sum(color[2] for color in colors) // count,
                        ),
                    )
                )
        if not batch:
            raise RuntimeError(f"cannot inpaint isolated mask in {bounds}")
        for x, y, color in batch:
            set_pixel(output, x, y, color)
            unresolved.remove((x, y))


def fill_horizontal_gradient(
    source: bytearray,
    output: bytearray,
    bounds: tuple[int, int, int, int],
    left_sample_x: int,
    right_sample_x: int,
) -> None:
    """Replace a rectangular interior from two untouched pixels per row."""
    x0, y0, x1, y1 = bounds
    denominator = max(1, x1 - x0 - 1)
    for y in range(y0, y1):
        left = pixel(source, left_sample_x, y)
        right = pixel(source, right_sample_x, y)
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


def luma(color: tuple[int, int, int]) -> int:
    return (54 * color[0] + 183 * color[1] + 19 * color[2]) // 256


def make_palette_mapper(colors: list[tuple[int, int, int]]):
    ordered = sorted((luma(color), color) for color in colors)
    levels = [entry[0] for entry in ordered]

    def mapped(color: tuple[int, int, int]) -> tuple[int, int, int]:
        target = luma(color)
        center = bisect.bisect_left(levels, target)
        lo = max(0, center - 5)
        hi = min(len(ordered), center + 6)
        candidates = ordered[lo:hi]
        if not candidates:
            return color
        weights = [max(1, 12 - abs(level - target)) for level, _ in candidates]
        total = sum(weights)
        return tuple(
            sum(candidate[channel] * weight
                for weight, (_, candidate) in zip(weights, candidates))
            // total
            for channel in range(3)
        )  # type: ignore[return-value]

    return mapped


def rgb565le(buffer: bytearray) -> bytes:
    output = bytearray(len(buffer) // 3 * 2)
    target = 0
    for source in range(0, len(buffer), 3):
        r, g, b = buffer[source : source + 3]
        value = ((r >> 3) << 11) | ((g >> 2) << 5) | (b >> 3)
        output[target] = value & 0xFF
        output[target + 1] = value >> 8
        target += 2
    return bytes(output)


def png_chunk(kind: bytes, payload: bytes) -> bytes:
    crc = zlib.crc32(kind)
    crc = zlib.crc32(payload, crc) & 0xFFFFFFFF
    return (
        struct.pack(">I", len(payload))
        + kind
        + payload
        + struct.pack(">I", crc)
    )


def encode_png(buffer: bytearray) -> bytes:
    rows = bytearray()
    stride = WIDTH * 3
    for y in range(HEIGHT):
        rows.append(0)
        rows.extend(buffer[y * stride : (y + 1) * stride])
    return (
        PNG_SIGNATURE
        + png_chunk(
            b"IHDR",
            struct.pack(">IIBBBBB", WIDTH, HEIGHT, 8, 2, 0, 0, 0),
        )
        + png_chunk(b"IDAT", zlib.compress(bytes(rows), level=9))
        + png_chunk(b"IEND", b"")
    )


def build_assets(root: Path) -> None:
    reference_path = root / "docs/assets/walle-theme-v9.png"
    clean_path = root / "docs/assets/walle-theme-v9-clean.png"
    theme_dir = root / "firmware/data/themes"
    mask_path = theme_dir / "walle_leaves_mask.bin"

    source = decode_png(reference_path)
    clean = bytearray(source)

    neutral_light = lambda color: (
        min(color) > 55 and max(color) - min(color) < 55
    )
    dark_on_yellow = lambda color: (
        color[0] < 175 and color[1] < 145 and color[2] < 80
    )
    dark_on_cream = lambda color: (
        color[0] < 175 and color[1] < 175 and color[2] < 175
    )
    cream_on_red = lambda color: color[1] > 115 and color[2] > 95
    entire_region = lambda _color: True

    regions = (
        ((20, 108, 145, 135), neutral_light, 3),
        ((18, 138, 301, 237), neutral_light, 6),
        ((350, 126, 431, 157), dark_on_yellow, 3),
        ((350, 156, 469, 224), dark_on_yellow, 6),
        ((70, 338, 207, 403), dark_on_cream, 6),
        ((280, 307, 446, 362), cream_on_red, 4),
        ((55, 433, 89, 463), entire_region, 0),
        ((319, 437, 346, 464), entire_region, 0),
        ((350, 437, 398, 462), entire_region, 0),
        ((400, 437, 429, 464), entire_region, 0),
    )
    # The battery interior is a smooth yellow field. Reconstruct it row by row
    # from untouched pixels on both sides of the original percentage text.
    # This keeps every anti-aliased outline pixel intact and avoids the dark
    # burrs that an inpaint mask can pull in from the frame.
    fill_horizontal_gradient(source, clean, (376, 35, 448, 60), 380, 445)
    for bounds, predicate, dilation in regions:
        inpaint_region(source, clean, bounds, predicate, dilation)

    mask = mask_path.read_bytes()
    if len(mask) != LEAF_W * LEAF_H:
        raise RuntimeError(
            f"leaf mask is {len(mask)} bytes; expected {LEAF_W * LEAF_H}"
        )

    active_colors: list[tuple[int, int, int]] = []
    inactive_colors: list[tuple[int, int, int]] = []
    for crop_index, leaf_index in enumerate(mask):
        if not leaf_index:
            continue
        x = LEAF_X + crop_index % LEAF_W
        y = LEAF_Y + crop_index // LEAF_W
        color = pixel(source, x, y)
        if leaf_index <= 7:
            active_colors.append(color)
        else:
            inactive_colors.append(color)
    if not active_colors or not inactive_colors:
        raise RuntimeError("leaf mask does not contain active and inactive leaves")

    to_active = make_palette_mapper(active_colors)
    to_inactive = make_palette_mapper(inactive_colors)
    active_crop = bytearray(LEAF_W * LEAF_H * 3)
    for crop_index, leaf_index in enumerate(mask):
        x = LEAF_X + crop_index % LEAF_W
        y = LEAF_Y + crop_index // LEAF_W
        color = pixel(source, x, y)
        active_color = color
        if leaf_index:
            if leaf_index <= 7:
                set_pixel(clean, x, y, to_inactive(color))
            else:
                active_color = to_active(color)
        target = crop_index * 3
        active_crop[target : target + 3] = bytes(active_color)

    clean_path.write_bytes(encode_png(clean))
    (theme_dir / "walle_bg.rgb565").write_bytes(rgb565le(clean))
    (theme_dir / "walle_leaves_active.rgb565").write_bytes(
        rgb565le(active_crop)
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
