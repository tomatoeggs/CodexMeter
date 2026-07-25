# Theme image assets

`animal_crossing_bg.rgb565` is the 480×480 static scene layer for the
`animal_crossing` firmware theme. It is RGB565 little-endian, row-major, without
a file header; the exact expected size is 480 × 480 × 2 = 460800 bytes.

The unadjusted original source is preserved at:

`docs/assets/animal-crossing-theme-clean-original.png`

The display-calibrated runtime source is:

`docs/assets/animal-crossing-theme-clean-display-70.png`

It is generated deterministically from the original with 70% saturation and
94% contrast. The original composition, dimensions and pixel alignment are not
changed. `animal_crossing_bg.rgb565` is the little-endian RGB565 conversion of
this calibrated source.

The approved visual reference with sample values remains at
`docs/assets/animal-crossing-theme-final.png`.

Runtime values, mode headings, quota cells, battery state and device status are
not baked into the runtime background. LVGL draws the live
`DashboardViewModel` transparently above this asset.

`gundam_bg.rgb565` is the 480×480 static White Base monitor and RX-78-2
illustration layer for the `gundam` firmware theme. It uses the same RGB565
little-endian, row-major, headerless format and exact 460800-byte size.

The clean runtime source is preserved at:

`docs/assets/gundam-theme-clean-original.png`

The approved visual reference with sample values remains at:

`docs/assets/gundam-theme-v2.png`

The clean source was produced from that reference by removing every
runtime-changing label, number, progress segment and status lamp while keeping
the RX-78-2 illustration, White Base identity, panel frames and background
texture. `gundam_bg.rgb565` is its direct little-endian RGB565 conversion.
LVGL draws all live headings, values, quota cells, battery, reset countdown,
task lamps and synchronization state above it.

`walle_bg.rgb565` is the 480×480 static mechanical layer for the `walle`
theme. Its clean source is:

`docs/assets/walle-theme-v9-clean.png`

It is derived deterministically from the approved V9 reference by clearing only
runtime-changing values, mode headings, task count and status lamps. The eye
housings, gradients, hazard stripe, quota arch, cassette, tread details, sprout
and all static labels retain the reference pixels.

`walle_leaves_active.rgb565` and `walle_leaves_mask.bin` preserve the exact nine
leaf silhouettes from the reference. The firmware recolors those pixels in the
loaded background according to the live seven-day remaining percentage; other
dashboard values remain LVGL overlays.

Regenerate the clean PNG and both RGB565 assets from the approved reference
with:

`python3 tools/generate_walle_theme_assets.py`
