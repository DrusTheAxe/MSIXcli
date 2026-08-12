# Step 3: Generate Open-Package Color Production Icons

Generate production master icons from the approved Step 1 open-package color families.

## Default Output Directory

When `GENERATION_MODE = PRIMARY`, save every generated image in:

`D:\source\repos\msixcli\images\graphic-design\step3`

The user may explicitly specify a different output directory.

When `GENERATION_MODE = ALL`, require the user to provide an explicit output directory before generation begins. Do not use `step3` or infer an output directory for `ALL`.

## Generation Configuration

Use the same generation configuration as Step 1:

```text
GENERATION_MODE = PRIMARY
PRIMARY_COLOR = cyan
```

Supported modes:

- `PRIMARY` — export only the configured primary color.
- `ALL` — export all 39 approved colors.

`PRIMARY` is the default mode.

The selected color set must match the Step 1 run being promoted to production. Do not silently export additional colors or omit selected colors.

In `PRIMARY` mode:

- No filename includes `-<color>`.
- Individual PNG filenames include `-<size>` only.

In `ALL` mode:

- SVG filenames include `-<color>`.
- Individual PNG filenames include both `-<color>` and `-<size>`.

## PNG Size Configuration

When `GENERATION_MODE = PRIMARY`, always generate every individual icon PNG at these sizes:

- `32x32`
- `48x48`
- `64x64`
- `96x96`
- `100x100`
- `256x256`
- `512x512`
- `1024x1024`

When `GENERATION_MODE = ALL`, require an explicit size selection before generation begins.

The user must either:

- Provide one or more explicit dimensions, or
- Specify `reference`, which means generate only `1024x1024`.

Do not infer sizes for `ALL`. Do not begin `ALL` generation without both an explicit output directory and an explicit size selection.

## Source of Truth

Follow:

`D:\source\repos\msixcli\images\graphic-design\.copilot\MSIXcli-Image-Generate-Step-1-Preview.md`

Use the approved individual SVG artwork in:

`D:\source\repos\msixcli\images\graphic-design\step1`

In `PRIMARY` mode, source filenames use:

- `msix.svg`
- `msixadmin.svg`
- `msixui.svg`
- `MSIXPropertySheet.svg`
- `MSIXTray.svg`

In `ALL` mode, source filenames use:

- `msix-<color>.svg`
- `msixadmin-<color>.svg`
- `msixui-<color>.svg`
- `MSIXPropertySheet-<color>.svg`
- `MSIXTray-<color>.svg`

Do not use family-sheet SVGs from `step1` as production sources.

## Locked Artwork

Copy each approved `step1` individual SVG unchanged.

Do not:

- Redraw, reinterpret, or approximate any artwork.
- Recolor any source.
- Change the outer package, flaps, interior opening, or inner cube.
- Change any top-, left-, or right-face glow geometry, clipping, opacity, position, or blur.
- Change package or cube geometry, perspective, scale, or placement.
- Change badge dimensions, placement, background, border, lighting, or shadow.
- Change any badge glyph.
- Add text, labels, borders, backgrounds, or comparison-sheet content.

The solid outer package walls must remain in front of the inner cube.

No part of the inner cube may appear through:

- The front wall.
- The left wall.
- The right wall.
- The lower package corner.

## Required Components

Generate every selected approved color for all five components:

| Component | Filename stem |
| --- | --- |
| `msix.exe` | `msix` |
| `msixadmin.exe` | `msixadmin` |
| `msixui.exe` | `msixui` |
| `MSIXPropertySheet.dll` | `MSIXPropertySheet` |
| `MSIXTray.exe` | `MSIXTray` |

## Required Colors

Generate production icons for these 39 normalized color names:

`red`, `green`, `blue`, `cyan`, `magenta`, `yellow`, `white`, `black`, `gray`, `orange`, `violet`, `pink`, `salmon`, `goldenrod`, `brown`, `sienna`, `maroon`, `wheat`, `coral`, `darkslategray`, `silver`, `purple`, `indigo`, `lime`, `fuchsia`, `gold`, `tortilla`, `sepia`, `ecru`, `espresso`, `mocha`, `coffee`, `walnut`, `brunette`, `sangria`, `shadow`, `charcoal`, `iron`, `hickory`.

Use the exact colors and derivation rules defined by Step 1.

Do not create alternate spellings such as:

- `bluw`
- `ywllow`
- `fuschia`

## Production Output

For every selected component, create:

- One matching SVG master.
- One transparent PNG master for every required or explicitly selected size.

In `PRIMARY` mode, preserve these Step 1 filenames exactly:

- `msix.svg`
- `msixadmin.svg`
- `msixui.svg`
- `MSIXPropertySheet.svg`
- `MSIXTray.svg`

For each required `PRIMARY` PNG size, preserve:

- `msix-<size>.png`
- `msixadmin-<size>.png`
- `msixui-<size>.png`
- `MSIXPropertySheet-<size>.png`
- `MSIXTray-<size>.png`

In `ALL` mode, preserve these Step 1 filenames exactly:

- `msix-<color>.svg`
- `msixadmin-<color>.svg`
- `msixui-<color>.svg`
- `MSIXPropertySheet-<color>.svg`
- `MSIXTray-<color>.svg`

For every explicitly selected `ALL` size, preserve:

- `msix-<color>-<size>.png`
- `msixadmin-<color>-<size>.png`
- `msixui-<color>-<size>.png`
- `MSIXPropertySheet-<color>-<size>.png`
- `MSIXTray-<color>-<size>.png`

Examples:

- `msix-32x32.png`
- `msix-1024x1024.png`
- `msixadmin-gold-64x64.png`
- `MSIXTray-fuchsia-256x256.png`

In `PRIMARY` mode, save all files in the Step 3 default output directory unless the user explicitly requests another location. In `ALL` mode, save all files only in the explicitly supplied output directory.

Do not copy or generate:

- `MSIXcli-icon-family.svg`
- `MSIXcli-icon-family.png`
- `MSIXcli-icon-family-<color>.svg`
- `MSIXcli-icon-family-<color>.png`
- Any side-by-side comparison sheet.

## PNG Requirements

Every PNG must be:

- Exactly the dimensions encoded in its `<size>` filename token.
- 32-bit RGBA.
- Rendered on a genuinely transparent background.
- Fully transparent in all four canvas corners.
- One icon only.
- Free of captions, labels, borders, and background fills.

Do not use a browser screenshot renderer if it replaces transparency with an opaque background.

## SVG Requirements

Every SVG must:

- Match every corresponding PNG size exactly, apart from raster dimensions.
- Remain editable vector artwork.
- Preserve the approved gradients, filters, clipping, and layer order.
- Contain no external file references.

## Required Counts

When `GENERATION_MODE = PRIMARY`, generate exactly:

- 5 SVG production masters.
- 40 PNG production masters: 5 components at 8 required sizes.
- 45 total production files.

When `GENERATION_MODE = ALL`, generate exactly:

- 195 SVG production masters.
- `195 × number of explicitly selected sizes` PNG production masters.
- `195 + (195 × number of explicitly selected sizes)` total production files.

When `ALL` uses `reference`, generate 195 SVG and 195 PNG production masters, for 390 total production files.

## Validation

After export, verify:

- `GENERATION_MODE`, `PRIMARY_COLOR`, and the selected PNG sizes match Step 1.
- `ALL` has an explicitly supplied output directory before generation begins.
- `ALL` has an explicitly supplied size selection before generation begins.
- `reference` resolves to only `1024x1024`.
- `PRIMARY` individual PNG filenames contain exact dimensions and no color suffix.
- `ALL` individual PNG filenames contain their normalized color and exact dimensions.
- SVG filenames follow their mode-specific naming rules.
- `PRIMARY` contains only the configured primary color.
- `ALL` contains all 39 normalized colors.
- Every selected color has all five components.
- PNG counts equal `selected colors × 5 components × selected sizes`.
- No family-sheet files exist in the Step 3 output directory.
- Every PNG matches the dimensions encoded in its filename.
- Every PNG contains a 32-bit alpha channel.
- All four corners of every PNG are fully transparent.
- Every SVG and its corresponding PNG depict identical artwork.
- The outer package and badge artwork remain unchanged across all colors.
- Only the inner cube and its approved three-face glow vary by color.
- The top, left, and right glow fields match the Step 1 geometry, clipping, opacity, and blur exactly.
- The inner cube remains centered with dark-blue interior space around every side.
- The package walls completely occlude the hidden portion of the cube.
- No cube pixels appear through any solid package wall or lower corner.
- Badge glyphs remain correct for all five components.
