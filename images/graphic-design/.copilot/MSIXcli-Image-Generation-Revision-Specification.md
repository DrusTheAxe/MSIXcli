# MSIXcli Image Generation Revision Specification

## Objective

Create a cohesive Microsoft\-style icon family for the MSIXcli tool suite. The blue MSIX package is the shared product identity; a small, consistently positioned badge identifies each tool's role.

---

## Global Icon\-Family Requirements

Apply the following requirements consistently to every tool icon:

- Use the exact same blue MSIX package artwork.
- Keep the same package size, perspective, angle, lighting, highlights, shadows, and placement.
- Keep the package as the primary visual element.
- Use the same badge size, position, shape, lighting, border treatment, corner radius, and shadow across the family.
- Place each role badge at the lower\-right of the package, matching the position used by the msix.exe terminal badge.
- Treat badges as small role indicators, not competing primary elements.
- Preserve clarity and visual recognition at 256x256, 64x64, 32x32, and 16x16 sizes.
- Maintain a clean Windows 11 Fluent\-style appearance.

---

## Family Relationship Constraint

The viewer should immediately recognize that all five icons belong to the same product family.

- The blue MSIX package should occupy approximately 80% of the visual weight.
- The role badge should occupy approximately 20% of the visual weight.
- The package should always be recognized before the badge.
- The blue package is the product identity.
- The badge only communicates the tool's role.

---

## Badge Consistency Requirements

All badges must:

- Use the same badge background color.
- Use the same badge dimensions.
- Use the same badge corner radius.
- Use the same badge border thickness.
- Use the same badge shadow.
- Use the same badge placement.
- Use the same visual style.

Badge differences are limited to the internal glyph only.

---

## Tool\-Specific Revisions

### msix.exe

- Use the blue MSIX package as the base icon.
- Use a small dark\-blue badge containing a clear white >\_ prompt.
- Retain this icon as the reference composition for package scale, lighting, perspective, badge size, and badge placement.

---

### msixadmin.exe

- Make the icon visually identical to msix.exe.
- Retain the same package and terminal badge without changing their size, perspective, lighting, shadow, or placement.
- Add a small, clean Windows UAC shield at the lower\-right corner of the terminal badge.
- Make the shield approximately 30% smaller than the shield used in previous revisions.
- Use the familiar Windows administrator shield shape with clean blue and yellow quadrants.
- Remove decorative borders, bevels, crests, umbrella\-like shapes, stars, and ornamental details.
- The shield must read strictly as a secondary elevation indicator.

---

### msixui.exe

- Make the icon visually identical to msix.exe except for the role badge.
- Replace the terminal badge with a Windows application\-window badge.
- Keep the exact same package artwork, perspective, size, lighting, highlights, shadow, and placement.
- Keep the application\-window badge in the exact same location and at the exact same size as the msix.exe terminal badge.
- Use one white application\-window rectangle with a simple title bar and a single content area.
- The badge contains exactly one window rectangle.
- Combine the previous two white rectangles into one white rectangle.
- Do not use multiple panes.
- Do not use split views.
- Do not use side\-by\-side rectangles.
- Do not use Windows\-logo quadrants.
- Do not use a large frame surrounding the package.
- The icon must read as the GUI counterpart to msix.exe, not as a separate product.

---

### MSIXPropertySheet.dll

- Use the same blue MSIX package base as the other tool icons.
- Replace the full\-page or full\-document composition with a small property\-sheet badge.
- Position and size the property\-sheet badge exactly like the msix.exe terminal badge.
- Use the same dark\-blue badge background as the other badges.

#### Property\-sheet Badge Appearance

- Display a small property\-page dialog.
- Display a single highlighted tab.
- Display 2–3 horizontal property lines.
- The property\-sheet badge must fit entirely inside the badge.
- Keep the badge readable at small icon sizes.
- Do not use a document as the primary icon.
- Do not use a full property dialog as the primary icon.
- The MSIX package remains the primary visual element.

---

### MSIXTray.exe

- Use the exact same blue MSIX package artwork as msix.exe.
- Use the exact same badge size, style, border, corner radius, shadow, and position as the msix.exe terminal badge.
- Replace the terminal glyph with a single activity\-monitor waveform glyph.

#### Badge Appearance

- Use a dark\-blue badge background.
- Use a single cyan waveform glyph.
- The waveform glyph is the ONLY symbol inside the badge.
- The waveform must occupy approximately 65% of the badge width.
- Use a waveform visually similar to:

\_\_/\\\_\_\_\_/\\\_\_

- Do not substitute any other glyph.

#### Do NOT Use

- Bells
- Notification icons
- Alarm icons
- Inbox icons
- Mail icons
- Message icons
- Toast\-notification metaphors
- Sound\-wave icons
- Speaker icons
- Wi-Fi icons
- Radar icons
- Broadcast icons
- Radiating rings
- Circles around the package
- Sparkles
- Stars
- Pulsing dots
- Secondary package overlays
- Heartbeat symbols outside the badge

#### Composition

The icon must be:

MSIX Package

      \+

 Activity Badge

The activity badge must be visually identical in size and placement to the msix.exe terminal badge.

The package remains the primary visual element.

#### Design Intent

The icon should communicate:

- Monitoring
- Telemetry
- Activity tracking
- Event observation

It should feel similar to:

- Performance Monitor
- Process Monitor
- WPA
- ETW tracing
- Diagnostics tooling

It should NOT feel similar to:

- Outlook
- Teams
- Notification Center
- Messaging applications
- Alerting applications

---

## Final Icon Mapping

| Tool | Base | Role Badge |
| --- | --- | --- |
| msix.exe | Blue MSIX package | Terminal prompt \(>\_\) |
| msixadmin.exe | Blue MSIX package | Terminal prompt \+ small UAC shield |
| msixui.exe | Blue MSIX package | Single application\-window badge |
| MSIXPropertySheet.dll | Blue MSIX package | Property\-sheet badge |
| MSIXTray.exe | Blue MSIX package | Activity\-waveform badge |

---

## Image Generation Constraint

Do not redesign the package separately for each tool.

All icons must look as though the exact same package artwork was duplicated and only the small lower\-right role badge was changed.

The resulting icon family must appear to be one coherent Windows tool suite.

The package artwork, lighting, perspective, scale, badge placement, and visual hierarchy must remain consistent across all icons.
