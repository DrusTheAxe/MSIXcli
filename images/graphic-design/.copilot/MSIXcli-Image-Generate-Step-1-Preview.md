# Step 1: Generate Open-Package Color Icon Previews

Generate Windows 11 Fluent-style color variants for the complete MSIXcli icon family.

## Default Output Directory

When `GENERATION_MODE = PRIMARY`, save every generated image in:

`D:\source\repos\msixcli\images\graphic-design\step1`

The user may explicitly specify a different output directory.

When `GENERATION_MODE = ALL`, require the user to provide an explicit output directory before generation begins. Do not use `step1` or infer an output directory for `ALL`.

## Generation Configuration

Support these generation modes:

- `PRIMARY` — generate only the configured primary color.
- `ALL` — generate every color in the approved palette.

Configuration:

```text
GENERATION_MODE = PRIMARY
PRIMARY_COLOR = cyan
```

`PRIMARY` is the default mode.

Resolve the selected color set before generating:

- When `GENERATION_MODE = PRIMARY`, selected colors = `cyan`.
- When `GENERATION_MODE = ALL`, selected colors = all 39 normalized palette colors.

Do not hard-code cyan throughout the generator. All primary-color selection must resolve through `PRIMARY_COLOR` so the primary color can be changed in one place later.

In `PRIMARY` mode:

- No filename includes `-<color>`.
- Individual PNG filenames include `-<size>` only.

In `ALL` mode:

- SVG and family-sheet filenames include `-<color>`.
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

## Canonical Artwork

Use the approved open-package artwork from:

- `D:\source\repos\msixcli\images\graphic-design\step1\msix.svg`
- `D:\source\repos\msixcli\images\graphic-design\step1\msixadmin.svg`
- `D:\source\repos\msixcli\images\graphic-design\step1\msixui.svg`
- `D:\source\repos\msixcli\images\graphic-design\step1\MSIXPropertySheet.svg`
- `D:\source\repos\msixcli\images\graphic-design\step1\MSIXTray.svg`

The canonical package contains:

- One solid blue outer package box.
- Four open package flaps.
- One dark interior opening.
- One centered inner cube.
- Enlarged clipped glow fields visible through the top, left, and right cube faces.
- Clear dark-blue interior space around every side of the inner cube.

There is no second inner box or nested package.

## Locked Package Geometry

For every generated icon, preserve exactly:

- Outer package geometry and perspective.
- Open-flap geometry and angle.
- Package colors, gradients, edge highlights, lighting, and shadows.
- Interior opening size, color, and placement.
- Inner-cube size, perspective, depth, and placement.
- Badge size, placement, background, border, corner radius, lighting, and shadow.
- Canvas scale and icon placement.

The solid outer package walls must be rendered in front of the inner cube.

The inner cube must never be visible through:

- The front wall.
- The left wall.
- The right wall.
- The lower package corner.

Do not add:

- Packing tape or ribbon.
- Windows logos.
- Labels, text, or barcodes.
- Additional boxes or cubes.
- Additional symbols inside the package.
- Transparent or translucent package walls.

## Permitted Color Changes

Only these inner-cube properties may change between color variants:

- Top-face gradient.
- Left-face gradient.
- Right-face gradient.
- Cube edge highlights.
- Top-, left-, and right-face glow color.
- Cube glow shadow.

Do not recolor the outer package, interior opening, role badge, or badge glyph.

Each color variant must retain:

- Darker cube faces.
- Bright glow visible through all three visible cube faces.
- A top-face glow ellipse with `rx=162` and `ry=85.5`.
- Left- and right-face glow ellipses with `rx=112.5` and `ry=118.5`.
- Face clipping that prevents each glow from extending outside its cube face.
- Cube-glow shadow blur with `stdDeviation=22.5`.
- The same three-face glow size, position, clipping, opacity, and blur.
- The same cube geometry and visibility.

## Icon Family

Generate every selected color for all five icons:

1. `msix`
2. `msixadmin`
3. `msixui`
4. `MSIXPropertySheet`
5. `MSIXTray`

Within each color family, the package and inner cube must be identical. Only the badge glyph differs.

### Badge Glyphs

#### msix

- Dark badge.
- White `>_` glyph.

#### msixadmin

- Same badge background, border, dimensions, lighting, and shadow as `msix`.
- Do not include the `>_` terminal glyph.
- Blue-and-yellow Windows UAC shield with a nominal height equal to 80% of the badge height.
- Make the shield 10% wider than its proportional width at that height.
- Center the shield horizontally and vertically inside the badge.
- The shield is the only glyph inside the badge.

#### msixui

- Exactly one white application-window rectangle.
- One title bar.
- One content area.
- No split panes, controls, sidebar, or Windows logo.

#### MSIXPropertySheet

- One highlighted tab.
- Exactly two horizontal property lines.
- No document, file, paper, or folded-corner symbol.

#### MSIXTray

- Cyan waveform only.
- Use the approved `__/\____/\__` waveform shape.
- No additional glyph or notification metaphor.

## Color Palette

Use these normalized color names and base values:

| Name | Base color |
| --- | --- |
| `red` | `#FF0000` |
| `green` | `#008000` |
| `blue` | `#0000FF` |
| `cyan` | `#00FFFF` |
| `magenta` | `#FF00FF` |
| `yellow` | `#FFFF00` |
| `white` | `#FFFFFF` |
| `black` | `#000000` |
| `gray` | `#808080` |
| `orange` | `#FFA500` |
| `violet` | `#EE82EE` |
| `pink` | `#FFC0CB` |
| `salmon` | `#FA8072` |
| `goldenrod` | `#DAA520` |
| `brown` | `#A52A2A` |
| `sienna` | `#A0522D` |
| `maroon` | `#800000` |
| `wheat` | `#F5DEB3` |
| `coral` | `#FF7F50` |
| `darkslategray` | `#2F4F4F` |
| `silver` | `#C0C0C0` |
| `purple` | `#800080` |
| `indigo` | `#4B0082` |
| `lime` | `#00FF00` |
| `fuchsia` | `#FF00FF` |
| `gold` | `#FFD700` |
| `tortilla` | `#9A7B4F` |
| `sepia` | `#704214` |
| `ecru` | `#C2B280` |
| `espresso` | `#4E312D` |
| `mocha` | `#967969` |
| `coffee` | `#6F4E37` |
| `walnut` | `#5C4033` |
| `brunette` | `#3B1E08` |
| `sangria` | `#5E1914` |
| `shadow` | `#8A795D` |
| `charcoal` | `#36454F` |
| `iron` | `#48494B` |
| `hickory` | `#8B5A2B` |

Do not create alternate spellings such as:

- `bluw`
- `ywllow`
- `fuschia`

## Output Naming

In `PRIMARY` mode, use:

- `msix.svg`
- `msixadmin.svg`
- `msixui.svg`
- `MSIXPropertySheet.svg`
- `MSIXTray.svg`
- `MSIXcli-icon-family.svg` and `MSIXcli-icon-family.png`

For each required `PRIMARY` PNG size, use:

- `msix-<size>.png`
- `msixadmin-<size>.png`
- `msixui-<size>.png`
- `MSIXPropertySheet-<size>.png`
- `MSIXTray-<size>.png`

Examples:

- `msix-32x32.png`
- `msix-1024x1024.png`
- `MSIXTray-256x256.png`

In `ALL` mode, generate one SVG for every icon and color.

Use:

- `msix-<color>.svg`
- `msixadmin-<color>.svg`
- `msixui-<color>.svg`
- `MSIXPropertySheet-<color>.svg`
- `MSIXTray-<color>.svg`

For every explicitly selected `ALL` size, use:

- `msix-<color>-<size>.png`
- `msixadmin-<color>-<size>.png`
- `msixui-<color>-<size>.png`
- `MSIXPropertySheet-<color>-<size>.png`
- `MSIXTray-<color>-<size>.png`

Examples:

- `msix-red-1024x1024.png`
- `msixadmin-gold-64x64.png`
- `MSIXTray-fuchsia-256x256.png`

In `ALL` mode, generate one side-by-side family sheet for every color:

- `MSIXcli-icon-family-<color>.svg`
- `MSIXcli-icon-family-<color>.png`

Each family sheet must:

- Show all five icons in the required order.
- Use a neutral light background.
- Label each icon with its component filename.
- Use identical icon scale and spacing.

## PNG Requirements

Each individual PNG must be:

- Exactly the dimensions encoded in its `<size>` filename token.
- 32-bit RGBA.
- Rendered on a transparent background.
- Fully transparent in all four canvas corners.
- One icon only, without labels or background fills.

Each family-sheet PNG must be:

- Exactly 1800x450 pixels.
- Rendered on the specified neutral light background.

## Required Counts

When `GENERATION_MODE = PRIMARY`, generate:

- 5 individual SVG icons.
- 40 individual PNG icons: 5 components at 8 required sizes.
- 1 family-sheet SVG file.
- 1 family-sheet PNG file.

When `GENERATION_MODE = ALL`, generate:

- 195 individual SVG icons.
- `195 × number of explicitly selected sizes` individual PNG icons.
- 39 family-sheet SVG files.
- 39 family-sheet PNG files.

When `ALL` uses `reference`, generate 195 individual PNG icons at `1024x1024`.

## Validation

After generation, verify:

- `GENERATION_MODE` is exactly `PRIMARY` or `ALL`.
- `PRIMARY_COLOR` is a normalized color in the approved palette.
- `ALL` has an explicitly supplied output directory before generation begins.
- `ALL` has an explicitly supplied size selection before generation begins.
- `reference` resolves to only `1024x1024`.
- `PRIMARY` individual PNG filenames contain exact dimensions and no color suffix.
- `ALL` individual PNG filenames contain their normalized color and exact dimensions.
- SVG and family-sheet filenames follow their mode-specific naming rules.
- The selected color set matches the configured generation mode.
- `PRIMARY` produces only the configured primary color.
- `ALL` produces all 39 normalized colors.
- Every selected color has all five icons.
- Every selected color has one family sheet.
- Actual PNG counts match `selected colors × 5 components × selected sizes`.
- Every individual PNG is 32-bit RGBA and matches the dimensions in its filename.
- Every individual PNG has four fully transparent corners.
- Every family sheet is 1800x450.
- The outer package and badge artwork are unchanged across all variants.
- The inner cube remains centered with dark-blue space visible around every side.
- The enlarged glow remains visible through the top, left, and right cube faces.
- All three glow fields retain their approved dimensions, clipping, opacity, and blur.
- The package walls completely occlude the hidden portion of the cube.
- No cube pixels appear through the solid sides or lower package corner.
- Only the inner cube and its glow change color.
