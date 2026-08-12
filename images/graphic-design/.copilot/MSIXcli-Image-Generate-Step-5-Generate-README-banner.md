# Step 5: Generate Open-Package Color README Banners

Generate one standard GitHub README banner and one RSN banner variant for every approved open-package color family.

## Default Output Directory

When `GENERATION_MODE = PRIMARY`, save every generated image in:

`D:\source\repos\msixcli\images\graphic-design\step5`

The user may explicitly specify a different output directory.

When `GENERATION_MODE = ALL`, require the user to provide an explicit output directory before generation begins. Do not use `step5` or infer an output directory for `ALL`.

## Generation Configuration

Use the same generation configuration as Steps 1, 3, and 4:

```text
GENERATION_MODE = PRIMARY
PRIMARY_COLOR = cyan
```

Supported modes:

- `PRIMARY` — generate only the configured primary-color banner.
- `ALL` — generate banners for all 39 approved colors.

`PRIMARY` is the default mode.

Only process colors present in both the selected Step 3 icon set and Step 4 logo set.

In `PRIMARY` mode, do not include `-<color>` in banner filenames. In `ALL` mode, include `-<color>`.

## Sources of Truth

Follow:

- `D:\source\repos\msixcli\images\graphic-design\.copilot\MSIXcli-Image-Generate-Step-4-Generate-project-logo.md`
- `D:\source\repos\msixcli\images\graphic-design\.copilot\MSIXcli-Image-Generate-Step-3-Generate-production-icons.md`

Use the approved SVG artwork from:

- Step 4 project logos in `D:\source\repos\msixcli\images\graphic-design\step4`
- Step 3 production icons in `D:\source\repos\msixcli\images\graphic-design\step3`
- Step 5 banner layout in `D:\source\repos\msixcli\images\graphic-design\step5\MSIXcli-README-banner.svg`

In `PRIMARY` mode, use:

- `MSIXcli-logo.svg`
- `msix.svg`
- `msixadmin.svg`
- `msixui.svg`
- `MSIXPropertySheet.svg`
- `MSIXTray.svg`

In `ALL` mode, use for each selected color:

- `MSIXcli-logo-<color>.svg`
- `msix-<color>.svg`
- `msixadmin-<color>.svg`
- `msixui-<color>.svg`
- `MSIXPropertySheet-<color>.svg`
- `MSIXTray-<color>.svg`

Use only matching colors within a banner. Never mix artwork from different color variants.

Do not use PNG files as source artwork when the matching SVG exists.

Use the approved Step 5 SVG as the immutable source for the banner canvas, background, layout, typography, spacing, divider, and decorative treatment. Replace only its embedded logo and icon artwork with the matching selected-color SVGs.

## Locked Artwork

Do not redraw, reinterpret, recolor, crop, or alter the logo or icons.

Preserve exactly:

- Outer package geometry and color.
- Open-flap geometry.
- Dark interior opening.
- Inner-cube geometry, color, placement, and enlarged clipped glow through the top, left, and right faces.
- Solid-wall occlusion.
- Badge geometry, placement, lighting, and glyphs.
- All gradients, filters, clipping, highlights, and shadows.

The package walls must continue to hide the lower portion of the inner cube.

No cube pixels may appear through any solid package wall or lower package corner.

## Required Colors

In `ALL` mode, generate a banner for these 39 normalized color names:

`red`, `green`, `blue`, `cyan`, `magenta`, `yellow`, `white`, `black`, `gray`, `orange`, `violet`, `pink`, `salmon`, `goldenrod`, `brown`, `sienna`, `maroon`, `wheat`, `coral`, `darkslategray`, `silver`, `purple`, `indigo`, `lime`, `fuchsia`, `gold`, `tortilla`, `sepia`, `ecru`, `espresso`, `mocha`, `coffee`, `walnut`, `brunette`, `sangria`, `shadow`, `charcoal`, `iron`, `hickory`.

Do not create alternate spellings such as:

- `bluw`
- `ywllow`
- `fuschia`

## Canvas and Background

Every banner must:

- Be exactly 1600x600 pixels.
- Use the approved dark-blue Windows-style gradient background.
- Use only a subtle low-contrast grid texture.
- Use rounded outer corners.
- Contain no photographs, screenshots, fictional windows, or invented UI.

The background must remain identical across all 39 color variants.

## Locked Layout

For every selected color:

- Place the matching `MSIXcli-logo.svg` or `MSIXcli-logo-<color>.svg` on the left, according to generation mode.
- Separate the logo area from the content with a subtle vertical divider.
- Place the title and subtitle in the upper content area.
- Place the feature list below the subtitle.
- Display all five matching Step 3 icons in one evenly spaced horizontal row beneath the text.

Preserve this icon order:

1. `msix`
2. `msixadmin`
3. `msixui`
4. `MSIXPropertySheet`
5. `MSIXTray`

The layout, scale, spacing, and text placement must be identical across every color banner.

## RSN Banner Variant

For every selected color, also generate an RSN banner variant.

The RSN banner must be identical to the standard banner except:

- Render the complete approved third icon, `msixui`, at 90% transparency (10% opacity).
- Render the complete approved fifth icon, `MSIXTray`, at 90% transparency (10% opacity).
- Preserve the original icon canvas, artwork, scale, placement, colors, gradients, shadows, badge, and glyph.
- Apply opacity uniformly to each complete icon.
- Add a subtle cyan glow behind and around each translucent icon.
- Use 5% glow opacity.
- Keep the glow soft and low intensity; it must not restore the apparent opacity of the icon or compete with the fully rendered icons.
- Keep the first, second, and fourth icons unchanged.

Do not redraw, simplify, outline, recolor, or otherwise alter the two translucent icons.

## Exact Text

Title:

`MSIXcli`

Subtitle:

`View, manage and monitor MSIX packages and packaged content`

Feature list, in this exact order:

`Inspect` · `Manage` · `Add/Update/Remove` · `(De)Provision` · `Repair` · `Reset` · `Monitor`

Feature-list spacing requirements:

- Keep all feature labels and separators on one line.
- Use identical visual spacing between every label and its adjacent separators.
- Measure or account for rendered text width.
- Do not position separators using guessed per-label coordinates.
- Prefer one flowing text element with preserved spaces and colored separator spans.

Do not add:

- Marketing copy.
- Slogans.
- Calls to action.
- Version numbers.
- URLs.
- Color names.
- Additional text.

## Output Naming

In `PRIMARY` mode, create:

- `MSIXcli-README-banner.svg`
- `MSIXcli-README-banner.png`
- `MSIXcli-README-banner-RSN.svg`
- `MSIXcli-README-banner-RSN.png`

In `ALL` mode, create for every color:

- `MSIXcli-README-banner-<color>.svg`
- `MSIXcli-README-banner-<color>.png`
- `MSIXcli-README-banner-RSN-<color>.svg`
- `MSIXcli-README-banner-RSN-<color>.png`

Examples:

- `MSIXcli-README-banner-red.png`
- `MSIXcli-README-banner-gold.png`
- `MSIXcli-README-banner-darkslategray.png`
- `MSIXcli-README-banner-mocha.png`
- `MSIXcli-README-banner-fuchsia.png`
- `MSIXcli-README-banner-RSN-cyan.png`

In `PRIMARY` mode, save all files in the Step 5 default output directory unless the user explicitly requests another location. In `ALL` mode, save all files only in the explicitly supplied output directory.

## SVG Requirements

Every SVG must:

- Be self-contained.
- Embed the matching Step 4 logo and all five matching Step 3 icons.
- Contain no external file references.
- Preserve the canonical artwork unchanged.
- Match its corresponding PNG exactly.

## PNG Requirements

Every PNG must:

- Be exactly 1600x600 pixels.
- Be 32-bit RGBA.
- Match its corresponding SVG.
- Preserve the approved rounded banner corners.

## Required Counts

When `GENERATION_MODE = PRIMARY`, generate exactly:

- 2 SVG README banners.
- 2 PNG README banners.
- 4 total banner files.

When `GENERATION_MODE = ALL`, generate exactly:

- 78 SVG README banners.
- 78 PNG README banners.
- 156 total banner files.

## Validation

After export, verify:

- `GENERATION_MODE` and `PRIMARY_COLOR` match Steps 1, 3, and 4.
- `ALL` has an explicitly supplied output directory before generation begins.
- `PRIMARY` filenames contain no color suffix.
- `ALL` filenames contain their normalized color suffix.
- `PRIMARY` contains only the configured primary-color standard and RSN banner pairs.
- `ALL` contains standard and RSN banner pairs for all 39 colors.
- Every selected color has one standard banner pair and one RSN banner pair.
- File counts equal `selected colors × 2 variants × 2 formats`.
- Every PNG is exactly 1600x600.
- Every banner uses one matching Step 4 logo.
- Every banner uses all five matching Step 3 icons exactly once.
- No banner mixes color variants.
- The icon order is correct.
- Every RSN banner renders only the third and fifth icons at 90% transparency (10% opacity).
- The translucent icons preserve their complete source artwork, scale, and placement.
- Each translucent icon has a subtle cyan halo behind and around its silhouette.
- No dotted, dashed, outlined, simplified, or recolored substitute artwork appears.
- The first, second, and fourth RSN icon positions match the standard banner exactly.
- The title, subtitle, and feature text match this specification exactly.
- Every feature label has consistent visual spacing from adjacent separators.
- The background and layout are identical across all colors.
- Every SVG is self-contained.
- The logo and icons match their canonical Step 3 and Step 4 sources.
- The solid package walls correctly occlude the inner cube in every embedded asset.
- No screenshots, fictional UI, color labels, extra text, or marketing content were introduced.
