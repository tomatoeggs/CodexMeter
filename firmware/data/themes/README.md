# Theme image assets

`animal_crossing_bg.rgb565` is the 480×480 static scene layer for the
`animal_crossing` firmware theme. It is RGB565 little-endian, row-major, without
a file header; the exact expected size is 480 × 480 × 2 = 460800 bytes.

The clean runtime background source is:

`docs/assets/animal-crossing-theme-clean-final.png`

The approved visual reference with sample values remains at
`docs/assets/animal-crossing-theme-final.png`.

Runtime values, mode headings, quota cells, battery state and device status are
not baked into the runtime background. LVGL draws the live
`DashboardViewModel` transparently above this asset.
