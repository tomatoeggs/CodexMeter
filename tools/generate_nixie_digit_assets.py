#!/usr/bin/env python3
"""Generate restrained, physically plausible Nixie digit glow assets."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
SOURCE_ATLAS = ROOT / "docs/assets/nixie-digit-atlas-alpha-v3.png"
BACKGROUND = ROOT / "docs/assets/nixie-theme-v6-static-background-texture-final.png"
ASSET_DIR = ROOT / "docs/assets"
THEME_DIR = ROOT / "firmware/data/themes"

CELL_W = 90
CELL_H = 108
DIGIT_POSITIONS = ((71, 130), (185, 130), (299, 130))
VISUAL_ALPHA_THRESHOLD = 80
TUBE_OPTICAL_Y_OFFSET = -3
CATHODE_SCALE_X = 1.10
CATHODE_SCALE_Y = 0.86


@dataclass(frozen=True)
class GlowPreset:
    close_radius: float
    close_opacity: float
    mid_radius: float
    mid_opacity: float
    outer_radius: float
    outer_opacity: float
    close_color: tuple[int, int, int] = (255, 105, 8)
    mid_color: tuple[int, int, int] = (232, 55, 3)
    outer_color: tuple[int, int, int] = (174, 34, 4)


PRESETS = {
    "restrained": GlowPreset(1.4, 0.26, 3.3, 0.055, 6.5, 0.010),
    "balanced": GlowPreset(1.7, 0.34, 3.8, 0.075, 7.0, 0.016),
    "warm": GlowPreset(2.0, 0.40, 4.4, 0.095, 7.8, 0.022),
    "design": GlowPreset(
        2.1,
        0.50,
        5.0,
        0.27,
        10.0,
        0.14,
        close_color=(255, 121, 10),
        mid_color=(255, 104, 6),
        outer_color=(214, 82, 5),
    ),
}


def multiply_mask(mask: Image.Image, opacity: float, modulation: np.ndarray) -> Image.Image:
    values = np.asarray(mask, dtype=np.float32) * opacity * modulation
    return Image.fromarray(np.clip(values, 0, 255).astype(np.uint8), "L")


def glow_modulation(size: tuple[int, int]) -> np.ndarray:
    """Return a stable, low-amplitude gas-density variation field."""
    rng = np.random.default_rng(46690)
    noise = rng.uniform(0.0, 1.0, (9, 19)).astype(np.float32)
    small = Image.fromarray(np.uint8(noise * 255), "L")
    field = small.resize(size, Image.Resampling.BICUBIC).filter(
        ImageFilter.GaussianBlur(5.0)
    )
    values = np.asarray(field, dtype=np.float32) / 255.0
    return 0.82 + values * 0.28


def reshape_digit_cells(source: Image.Image) -> Image.Image:
    """Match the lit cathodes to the wider, shorter wire forms in the tubes."""
    source = source.convert("RGBA")
    reshaped = Image.new("RGBA", source.size, (0, 0, 0, 0))
    scaled_w = int(round(CELL_W * CATHODE_SCALE_X))
    scaled_h = int(round(CELL_H * CATHODE_SCALE_Y))

    for digit in range(10):
        column = digit % 5
        row = digit // 5
        left = column * CELL_W
        top = row * CELL_H
        cell = source.crop((left, top, left + CELL_W, top + CELL_H))
        cell = cell.resize((scaled_w, scaled_h), Image.Resampling.LANCZOS)
        x = left + (CELL_W - scaled_w) // 2
        y = top + (CELL_H - scaled_h) // 2
        reshaped.alpha_composite(cell, (x, y))

    return reshaped


def center_digit_cells(source: Image.Image) -> tuple[Image.Image, list[tuple[int, int]]]:
    """Center each cathode by its visible envelope, not transparent cell bounds."""
    source = source.convert("RGBA")
    centered = Image.new("RGBA", source.size, (0, 0, 0, 0))
    shifts: list[tuple[int, int]] = []
    target_x = (CELL_W - 1) / 2.0
    # The illustrated mesh sits four pixels above the nominal 108 px sprite
    # cell center, so use the tube's optical center rather than the cell center.
    target_y = (CELL_H - 1) / 2.0 + TUBE_OPTICAL_Y_OFFSET

    for digit in range(10):
        column = digit % 5
        row = digit // 5
        left = column * CELL_W
        top = row * CELL_H
        cell = source.crop((left, top, left + CELL_W, top + CELL_H))
        visible = cell.getchannel("A").point(
            lambda value: 255 if value >= VISUAL_ALPHA_THRESHOLD else 0
        )
        bounds = visible.getbbox()
        if bounds is None:
            shifts.append((0, 0))
            centered.alpha_composite(cell, (left, top))
            continue

        center_x = (bounds[0] + bounds[2] - 1) / 2.0
        center_y = (bounds[1] + bounds[3] - 1) / 2.0
        dx = int(round(target_x - center_x))
        dy = int(round(target_y - center_y))
        shifts.append((dx, dy))
        centered.alpha_composite(cell, (left + dx, top + dy))

    return centered, shifts


def make_atlas(source: Image.Image, preset: GlowPreset) -> Image.Image:
    source = source.convert("RGBA")
    alpha = np.asarray(source.getchannel("A"), dtype=np.float32)

    # Only the bright cathode is a light source. The source atlas already
    # contains a small optical fringe, so thresholding it prevents that fringe
    # from recursively turning into the broad red disks seen in the old asset.
    core = np.clip((alpha - 150.0) / 79.0, 0.0, 1.0)
    core = Image.fromarray(np.uint8(core * 255), "L")
    modulation = glow_modulation(source.size)

    result = Image.new("RGBA", source.size, (0, 0, 0, 0))
    layers = (
        (preset.outer_radius, preset.outer_opacity, preset.outer_color),
        (preset.mid_radius, preset.mid_opacity, preset.mid_color),
        (preset.close_radius, preset.close_opacity, preset.close_color),
    )
    for radius, opacity, color in layers:
        blurred = core.filter(ImageFilter.GaussianBlur(radius))
        layer_alpha = multiply_mask(blurred, opacity, modulation)
        layer = Image.new("RGBA", source.size, color + (0,))
        layer.putalpha(layer_alpha)
        result = Image.alpha_composite(result, layer)

    return Image.alpha_composite(result, source)


def make_preview(background: Image.Image, atlas: Image.Image) -> Image.Image:
    preview = background.convert("RGBA")
    for digit, position in zip((0, 8, 3), DIGIT_POSITIONS):
        column = digit % 5
        row = digit // 5
        cell = atlas.crop(
            (column * CELL_W, row * CELL_H, (column + 1) * CELL_W, (row + 1) * CELL_H)
        )
        preview.alpha_composite(cell, position)
    return preview.convert("RGB")


def write_raw_digits(atlas: Image.Image) -> None:
    THEME_DIR.mkdir(parents=True, exist_ok=True)
    for digit in range(10):
        column = digit % 5
        row = digit // 5
        cell = atlas.crop(
            (column * CELL_W, row * CELL_H, (column + 1) * CELL_W, (row + 1) * CELL_H)
        )
        # LVGL ARGB8888 is stored as BGRA bytes on this little-endian target.
        data = cell.convert("RGBA").tobytes("raw", "BGRA")
        (THEME_DIR / f"nixie_digit_{digit}.argb8888").write_bytes(data)


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--install",
        choices=PRESETS,
        help="also install the selected atlas and ten firmware sprites",
    )
    args = parser.parse_args()

    source = reshape_digit_cells(Image.open(SOURCE_ATLAS))
    source, shifts = center_digit_cells(source)
    print(
        "digit visual-center shifts: "
        + " ".join(f"{digit}:{dx:+d},{dy:+d}" for digit, (dx, dy) in enumerate(shifts))
    )
    background = Image.open(BACKGROUND)
    generated: dict[str, Image.Image] = {}
    for name, preset in PRESETS.items():
        atlas = make_atlas(source, preset)
        # Recheck the rendered envelope because asymmetric cathode strokes can
        # move the thresholded edge by one pixel after glow compositing.
        atlas, _ = center_digit_cells(atlas)
        generated[name] = atlas
        atlas.save(ASSET_DIR / f"nixie-digit-atlas-halo-{name}.png")
        make_preview(background, atlas).save(
            ASSET_DIR / f"nixie-theme-halo-preview-{name}.png"
        )

    if args.install:
        final = generated[args.install]
        final.save(ASSET_DIR / "nixie-digit-atlas-halo-final.png")
        write_raw_digits(final)


if __name__ == "__main__":
    main()
