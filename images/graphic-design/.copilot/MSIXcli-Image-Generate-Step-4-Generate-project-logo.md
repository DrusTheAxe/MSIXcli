# Step 4: Generate Open-Package Color Project Logos

Generate project-logo variants from the approved Step 3 open-package production icons.

## Default Output Directory

When `GENERATION_MODE = PRIMARY`, save every generated image in:

`D:\source\repos\msixcli\images\graphic-design\step4`

The user may explicitly specify a different output directory.

When `GENERATION_MODE = ALL`, require the user to provide an explicit output directory before generation begins. Do not use `step4` or infer an output directory for `ALL`.

## Generation Configuration

Use the same generation configuration as Steps 1 and 3:

```text
GENERATION_MODE = PRIMARY
PRIMARY_COLOR = cyan
```

Supported modes:

- `PRIMARY` — generate only the configured primary-color logo.
- `ALL` — generate logos for all 39 approved colors.

`PRIMARY` is the default mode.

Only process colors present in the selected Step 3 production set. Always retain the color in logo filenames, including `PRIMARY` mode.

## Source of Truth

Follow:

`D:\source\repos\msixcli\images\graphic-design\.copilot\MSIXcli-Image-Generate-Step-3-Generate-production-icons.md`

Use only the approved `msix` SVG production masters in:

`D:\source\repos\msixcli\images\graphic-design\step3`

Source filenames use:

`msix-<color>.svg`

Do not use:

- PNG files as source artwork.
- Other component icons.
- Family sheets.
- Approved `step1` artwork when the corresponding Step 3 production master exists.

## Logo Derivation

For each selected color, create the project logo by removing only:

- The terminal badge.
- The terminal badge shadow.
- The white `>_` glyph.

Preserve the complete package artwork unchanged.

The resulting logo contains:

- One solid blue outer package box.
- Four open package flaps.
- One dark interior opening.
- One centered colored inner cube.
- One concentrated glow centered inside the cube's top face.
- Clear dark-blue interior space around every side of the cube.
- No badge, glyph, text, or background.

## Locked Artwork

Do not:

- Redraw, reinterpret, or approximate the package.
- Recolor any source.
- Change the outer package, flaps, interior opening, or inner cube.
- Change the centered glow.
- Change package or cube geometry, perspective, scale, or placement.
- Change gradients, edge highlights, lighting, filters, clipping, shadows, or layer order.
- Scale, reposition, crop, or rotate the package.
- Add text, badges, borders, backgrounds, symbols, or decorations.

The solid outer package walls must remain in front of the inner cube.

No part of the inner cube may appear through:

- The front wall.
- The left wall.
- The right wall.
- The lower package corner.

## Required Colors

Generate a logo for these 39 normalized color names:

`red`, `green`, `blue`, `cyan`, `magenta`, `yellow`, `white`, `black`, `gray`, `orange`, `violet`, `pink`, `salmon`, `goldenrod`, `brown`, `sienna`, `maroon`, `wheat`, `coral`, `darkslategray`, `silver`, `purple`, `indigo`, `lime`, `fuchsia`, `gold`, `tortilla`, `sepia`, `ecru`, `espresso`, `mocha`, `coffee`, `walnut`, `brunette`, `sangria`, `shadow`, `charcoal`, `iron`, `hickory`.

Preserve the exact color treatment from each corresponding Step 3 source.

Do not create alternate spellings such as:

- `bluw`
- `ywllow`
- `fuschia`

## Output Naming

For every color, create:

- `MSIXcli-logo-<color>.svg`
- `MSIXcli-logo-<color>.png`

Examples:

- `MSIXcli-logo-red.png`
- `MSIXcli-logo-gold.png`
- `MSIXcli-logo-darkslategray.png`
- `MSIXcli-logo-mocha.png`
- `MSIXcli-logo-fuchsia.png`

In `PRIMARY` mode, save all files in the Step 4 default output directory unless the user explicitly requests another location. In `ALL` mode, save all files only in the explicitly supplied output directory.

## PNG Requirements

Every PNG must be:

- Exactly 1024x1024 pixels.
- 32-bit RGBA.
- Rendered on a genuinely transparent background.
- Fully transparent in all four canvas corners.
- Free of badges, glyphs, labels, captions, borders, and background fills.
- Suitable for use as a GitHub project avatar.

Do not use a browser screenshot renderer if it replaces transparency with an opaque background.

## SVG Requirements

Every SVG must:

- Match its PNG exactly.
- Remain editable vector artwork.
- Preserve the approved gradients, filters, clipping, and layer order.
- Contain no external file references.
- Contain no terminal badge or badge glyph.

## Required Counts

When `GENERATION_MODE = PRIMARY`, generate exactly:

- 1 SVG project-logo master.
- 1 PNG project-logo master.
- 2 total logo files.

When `GENERATION_MODE = ALL`, generate exactly:

- 39 SVG project-logo masters.
- 39 PNG project-logo masters.
- 78 total logo files.

## Validation

After export, verify:

- `GENERATION_MODE` and `PRIMARY_COLOR` match Steps 1 and 3.
- `ALL` has an explicitly supplied output directory before generation begins.
- `PRIMARY` contains only the configured primary-color logo.
- `ALL` contains all 39 color logos.
- Every selected color has one SVG and one PNG logo.
- File counts equal `selected colors × 2 formats`.
- Every PNG is exactly 1024x1024.
- Every PNG contains a 32-bit alpha channel.
- All four corners of every PNG are fully transparent.
- Every SVG and corresponding PNG depict identical artwork.
- No badge, badge shadow, badge glyph, text, or background remains.
- The outer package artwork is identical across all colors.
- Only the inner cube and its centered glow differ by color.
- Each logo exactly matches its corresponding Step 3 `msix-<color>.svg` package artwork.
- The inner cube remains centered with dark-blue interior space around every side.
- The package walls completely occlude the hidden portion of the cube.
- No cube pixels appear through any solid package wall or lower corner.
