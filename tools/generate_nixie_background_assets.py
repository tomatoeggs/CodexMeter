#!/usr/bin/env python3
"""Generate locally lifted Nixie tube backgrounds without brightening the UI."""

from __future__ import annotations

import argparse
from dataclasses import dataclass
from pathlib import Path

import numpy as np
from PIL import Image, ImageDraw, ImageFilter


ROOT = Path(__file__).resolve().parents[1]
ASSET_DIR = ROOT / "docs/assets"
THEME_DIR = ROOT / "firmware/data/themes"
SOURCE = ASSET_DIR / "nixie-theme-v6-static-background-texture-final.png"
DEFAULT_ATLAS = ASSET_DIR / "nixie-digit-atlas-halo-final.png"

CELL_W = 90
CELL_H = 108
DIGIT_POSITIONS = ((71, 130), (185, 130), (299, 130))

# The percent sign is part of the static artwork. Move a small padded patch so
# its anti-aliased edge and engraved shadow stay intact, then backfill the
# vacated strip with the immediately adjacent background texture.
PERCENT_PATCH = (431, 205, 456, 231)
PERCENT_SHIFT_X = 3

# The normal Today counter has a narrow decimal roller between its second and
# third numeric rollers.  Values from 100M through 999M do not have a decimal
# digit, so the firmware overlays this alternate four-cell interior instead:
# three equal numeric rollers followed by one equal unit roller.
TODAY_INTEGER_PANEL = (47, 337, 203, 392)
TODAY_INTEGER_CELL_SOURCES = ((47, 86), (86, 125), (137, 176), (137, 176))


@dataclass(frozen=True)
class TubeLightPreset:
    gamma: float
    strength: float
    red_scale: float
    green_scale: float
    blue_scale: float
    warm_lift: tuple[float, float, float]
    ambient_warm: float = 0.0


PRESETS = {
    "a": TubeLightPreset(0.91, 0.58, 1.035, 1.012, 0.975, (1.8, 0.8, 0.0)),
    "b": TubeLightPreset(0.86, 0.72, 1.050, 1.018, 0.960, (2.6, 1.2, 0.0)),
    "c": TubeLightPreset(0.81, 0.84, 1.065, 1.024, 0.945, (3.5, 1.6, 0.0)),
    "design": TubeLightPreset(
        0.95, 0.40, 1.120, 0.960, 0.820, (1.0, 0.0, 0.0), ambient_warm=0.9
    ),
}


def shift_percent_sign(source: Image.Image) -> Image.Image:
    output = source.convert("RGB").copy()
    left, top, right, bottom = PERCENT_PATCH
    patch = output.crop(PERCENT_PATCH)
    backfill = output.crop((left - PERCENT_SHIFT_X, top, left, bottom))
    output.paste(backfill, (left, top))
    output.paste(patch, (left + PERCENT_SHIFT_X, top))
    return output


def tube_mask(size: tuple[int, int]) -> Image.Image:
    mask = Image.new("L", size, 0)
    draw = ImageDraw.Draw(mask)
    for bounds in ((63, 61, 169, 285), (177, 61, 283, 285), (291, 61, 397, 285)):
        draw.ellipse(bounds, fill=255)
    return mask.filter(ImageFilter.GaussianBlur(7.0))


def ambient_glow_mask(size: tuple[int, int]) -> Image.Image:
    mask = Image.new("L", size, 0)
    draw = ImageDraw.Draw(mask)
    for bounds in ((76, 111, 156, 263), (190, 111, 270, 263), (304, 111, 384, 263)):
        draw.ellipse(bounds, fill=210)
    return mask.filter(ImageFilter.GaussianBlur(20.0))


def light_tubes(source: Image.Image, preset: TubeLightPreset) -> Image.Image:
    source_rgb = np.asarray(source.convert("RGB"), dtype=np.float32)
    normalized = source_rgb / 255.0
    lifted = np.power(normalized, preset.gamma) * 255.0
    lifted *= np.array(
        [preset.red_scale, preset.green_scale, preset.blue_scale], dtype=np.float32
    )
    lifted += np.array(preset.warm_lift, dtype=np.float32)

    mask = np.asarray(tube_mask(source.size), dtype=np.float32)[:, :, None] / 255.0
    luminance = (
        source_rgb[:, :, 0] * 0.2126
        + source_rgb[:, :, 1] * 0.7152
        + source_rgb[:, :, 2] * 0.0722
    )
    # Preserve black gaps while lifting reflective glass and metal detail.
    detail_weight = 0.18 + 0.82 * np.clip((luminance - 5.0) / 52.0, 0.0, 1.0)
    mask *= preset.strength * detail_weight[:, :, None]
    output = source_rgb * (1.0 - mask) + np.clip(lifted, 0, 255) * mask
    if preset.ambient_warm:
        ambient = (
            np.asarray(ambient_glow_mask(source.size), dtype=np.float32)[:, :, None]
            / 255.0
        )
        output += ambient * preset.ambient_warm * np.array(
            (6.0, 2.2, 0.0), dtype=np.float32
        )
    return Image.fromarray(np.uint8(np.clip(output, 0, 255)), "RGB")


def add_digits(background: Image.Image, atlas: Image.Image) -> Image.Image:
    preview = background.convert("RGBA")
    atlas = atlas.convert("RGBA")
    for digit, position in zip((0, 8, 3), DIGIT_POSITIONS):
        column = digit % 5
        row = digit // 5
        cell = atlas.crop(
            (column * CELL_W, row * CELL_H, (column + 1) * CELL_W, (row + 1) * CELL_H)
        )
        preview.alpha_composite(cell, position)
    return preview.convert("RGB")


def rgb565le(image: Image.Image) -> bytes:
    rgb = np.asarray(image.convert("RGB"), dtype=np.uint16)
    packed = ((rgb[:, :, 0] >> 3) << 11) | ((rgb[:, :, 1] >> 2) << 5) | (
        rgb[:, :, 2] >> 3
    )
    return packed.astype("<u2").tobytes()


def build_today_integer_panel(background: Image.Image) -> Image.Image:
    left, top, right, bottom = TODAY_INTEGER_PANEL
    output = Image.new("RGB", (right - left, bottom - top))
    cell_width = output.width // len(TODAY_INTEGER_CELL_SOURCES)
    for index, (source_left, source_right) in enumerate(
        TODAY_INTEGER_CELL_SOURCES
    ):
        cell = background.crop((source_left, top, source_right, bottom))
        if cell.width != cell_width:
            cell = cell.resize(
                (cell_width, output.height), Image.Resampling.LANCZOS
            )
        output.paste(cell, (index * cell_width, 0))
    return output


def main() -> None:
    parser = argparse.ArgumentParser()
    parser.add_argument(
        "--install",
        choices=PRESETS,
        help="also install the selected background as firmware RGB565",
    )
    parser.add_argument("--atlas", type=Path, default=DEFAULT_ATLAS)
    args = parser.parse_args()

    source = shift_percent_sign(Image.open(SOURCE))
    atlas = Image.open(args.atlas)
    generated: dict[str, Image.Image] = {}
    for name, preset in PRESETS.items():
        background = light_tubes(source, preset)
        generated[name] = background
        background.save(ASSET_DIR / f"nixie-theme-v6-static-background-tube-lit-{name}.png")
        add_digits(background, atlas).save(
            ASSET_DIR / f"nixie-theme-tube-lit-preview-{name}.png"
        )

    if args.install:
        final = generated[args.install]
        final.save(ASSET_DIR / "nixie-theme-v6-static-background-tube-lit-final.png")
        THEME_DIR.mkdir(parents=True, exist_ok=True)
        (THEME_DIR / "nixie_bg.rgb565").write_bytes(rgb565le(final))
        integer_panel = build_today_integer_panel(final)
        integer_panel.save(ASSET_DIR / "nixie-theme-today-integer-panel.png")
        (THEME_DIR / "nixie_today_integer.rgb565").write_bytes(
            rgb565le(integer_panel)
        )


if __name__ == "__main__":
    main()
