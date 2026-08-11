# Step 1: Generate Open-Package Color Icon Families

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

Always retain the color in output filenames, including `PRIMARY` mode.

## Canonical Artwork

Use the approved open-package artwork from:

- `D:\source\repos\msixcli\images\graphic-design\step1\msix-cyan.svg`
- `D:\source\repos\msixcli\images\graphic-design\step1\msixadmin-cyan.svg`
- `D:\source\repos\msixcli\images\graphic-design\step1\msixui-cyan.svg`
- `D:\source\repos\msixcli\images\graphic-design\step1\MSIXPropertySheet-cyan.svg`
- `D:\source\repos\msixcli\images\graphic-design\step1\MSIXTray-cyan.svg`

The canonical package contains:

- One solid blue outer package box.
- Four open package flaps.
- One dark interior opening.
- One centered inner cube.
- A concentrated glow centered inside the top face of the inner cube.
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
- Centered glow color.
- Cube glow shadow.

Do not recolor the outer package, interior opening, role badge, or badge glyph.

Each color variant must retain:

- Darker cube faces.
- A brighter glow centered inside the cube's top face.
- The same glow size and position.
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

- Same terminal badge as `msix`.
- Small blue-and-yellow Windows UAC shield.
- Shield remains entirely inside the badge.

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

Generate one SVG and one PNG for every icon and color.

Use:

- `msix-<color>.svg` and `msix-<color>.png`
- `msixadmin-<color>.svg` and `msixadmin-<color>.png`
- `msixui-<color>.svg` and `msixui-<color>.png`
- `MSIXPropertySheet-<color>.svg` and `MSIXPropertySheet-<color>.png`
- `MSIXTray-<color>.svg` and `MSIXTray-<color>.png`

Examples:

- `msix-red.png`
- `msixadmin-gold.png`
- `msixui-darkslategray.png`
- `MSIXPropertySheet-mocha.png`
- `MSIXTray-fuchsia.png`

Generate one side-by-side family sheet for every color:

- `MSIXcli-icon-family-<color>.svg`
- `MSIXcli-icon-family-<color>.png`

Each family sheet must:

- Show all five icons in the required order.
- Use a neutral light background.
- Label each icon with its component filename.
- Use identical icon scale and spacing.

## PNG Requirements

Each individual PNG must be:

- Exactly 1024x1024 pixels.
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
- 5 individual PNG icons.
- 1 family-sheet SVG file.
- 1 family-sheet PNG file.

When `GENERATION_MODE = ALL`, generate:

- 195 individual SVG icons.
- 195 individual PNG icons.
- 39 family-sheet SVG files.
- 39 family-sheet PNG files.

## Validation

After generation, verify:

- `GENERATION_MODE` is exactly `PRIMARY` or `ALL`.
- `PRIMARY_COLOR` is a normalized color in the approved palette.
- `ALL` has an explicitly supplied output directory before generation begins.
- The selected color set matches the configured generation mode.
- `PRIMARY` produces only the configured primary color.
- `ALL` produces all 39 normalized colors.
- Every selected color has all five icons.
- Every selected color has one family sheet.
- Actual file counts match `selected colors × components × formats`, plus one SVG/PNG family sheet per selected color.
- Every individual PNG is 1024x1024 RGBA.
- Every individual PNG has four fully transparent corners.
- Every family sheet is 1800x450.
- The outer package and badge artwork are unchanged across all variants.
- The inner cube remains centered with dark-blue space visible around every side.
- The glow remains centered inside the cube.
- The package walls completely occlude the hidden portion of the cube.
- No cube pixels appear through the solid sides or lower package corner.
- Only the inner cube and its glow change color.
