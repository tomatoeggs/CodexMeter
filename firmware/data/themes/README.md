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

`walle_v10_bg.rgb565` is the full-screen static scene for the `walle_v10`
firmware theme, whose user-facing name is `WALL-E EARTH`. Its approved reference
and clean runtime source are:

`docs/assets/walle-theme-v10.png`

`docs/assets/walle-theme-v10-clean.png`

The clean scene retains the sunset city, WALL-E illustration, sprout boot,
mechanical panels, cassette and tread texture. LVGL draws all changing headings,
values, battery text, reset countdown and task count above it.

`walle_v10_leaves_active.rgb565` and
`walle_v10_leaves_inactive.rgb565` contain palette-matched variants of the ten
quota leaves. `walle_v10_leaves_mask.bin` identifies each leaf independently so
the firmware can render 90%–99% as nine active leaves and one inactive leaf,
while reserving ten active leaves for 100%. The generator normalizes both states
across all slots so leaves extracted from the inactive part of the reference do
not appear falsely disabled after activation.

Regenerate the WALL-E EARTH clean PNG and runtime assets with:

`python3 tools/generate_walle_v10_theme_assets.py`

`walle_blueprint_bg.rgb565` is the full-screen static engineering drawing for
the `walle_blueprint` firmware theme, whose user-facing name is
`WALL-E BLUEPRINT`. Its approved reference and clean runtime source are:

`docs/assets/walle-theme-v15-refined-v4.png`

`docs/assets/walle-theme-blueprint-clean.png`

The clean scene preserves the warm-gold frames, camera pod, exploded WALL-E
body, arms, sprout, triangular treads and all fixed captions. Its visible navy
paper is rebuilt as one continuous field behind the preserved artwork, instead
of filling each erased text rectangle independently, so live labels do not sit
on subtly mismatched patches. LVGL draws live values, mode headings, battery
text, reset countdown, and the three resized footer capsules with their
consistently scaled labels and lamps.

`walle_blueprint_bar_active.rgb565` and
`walle_blueprint_bar_inactive.rgb565` contain the two palette states for the
ten quota cells. `walle_blueprint_bar_mask.bin` identifies every cell so
the firmware can update only the affected pixels in the PSRAM-backed background.
Each cell is 18x22 pixels with a fixed three-pixel gap and a shared row palette,
so every visible block has identical geometry. At 95%, nine cells are active and
one is inactive; 100% activates all ten.

`walle_blueprint_battery_active.rgb565`,
`walle_blueprint_battery_inactive.rgb565` and
`walle_blueprint_battery_mask.bin` provide seven dynamic battery cells. The
outer icon remains in the static blueprint, while the fill follows the live
battery percentage; 100% activates all seven cells.

Regenerate the WALL-E BLUEPRINT clean PNG and runtime assets with:

`python3 tools/generate_walle_blueprint_theme_assets.py`
