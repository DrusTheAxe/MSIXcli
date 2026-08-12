# MSIXcli Image Generation Specification

## Purpose

This document defines the overall MSIXcli image-generation workflow.

Run the numbered steps in ascending order. The detailed requirements, generation instructions, and validation criteria in each numbered step file are authoritative.

Do not duplicate the complete step specifications in this document. Update the relevant numbered step file when its requirements change.

## Plan Location and Naming

The workflow plans are stored in:

`D:\source\repos\msixcli\images\graphic-design\.copilot`

Plan filenames use:

`MSIXcli-Image-Generate-Step-<n>-<name>.md`

Where:

- `<n>` is the execution order.
- `<name>` is a short description of the work performed by that step.

## Execution Flow

Execute every required step in order:

1. [Generate icon previews](MSIXcli-Image-Generate-Step-1-Preview.md)
   - Generate the approved open-package component icons and color families.
   - Default `PRIMARY` output: `D:\source\repos\msixcli\images\graphic-design\step1`
2. [Approve icon previews](MSIXcli-Image-Generate-Step-2-Approve.md)
   - Review the Step 1 artwork before allowing downstream production work.
   - Read artwork from `step1`.
   - Save generated review images in `step2` unless another location is explicitly requested.
3. [Generate production icons](MSIXcli-Image-Generate-Step-3-Generate-production-icons.md)
   - Promote approved Step 1 component artwork into production SVG and PNG masters.
   - Default `PRIMARY` output: `D:\source\repos\msixcli\images\graphic-design\step3`
4. [Generate the project logo](MSIXcli-Image-Generate-Step-4-Generate-project-logo.md)
   - Derive the badge-free project logo from approved Step 3 artwork.
   - Default `PRIMARY` output: `D:\source\repos\msixcli\images\graphic-design\step4`
5. [Generate the README banners](MSIXcli-Image-Generate-Step-5-Generate-README-banner.md)
   - Generate the standard banner and the RSN variant from approved Step 3 icons and the Step 4 logo.
   - In the RSN variant, the third and fifth icons are 90% transparent with a subtle 5% cyan glow.
   - Default `PRIMARY` output: `D:\source\repos\msixcli\images\graphic-design\step5`

Do not run a later step until all required generation and validation work in the preceding step is complete.

## Shared Generation Configuration

Image-generation steps support:

```text
GENERATION_MODE = PRIMARY
PRIMARY_COLOR = cyan
```

- `PRIMARY` generates only `PRIMARY_COLOR`.
- `ALL` generates all approved palette colors.
- `PRIMARY` is the default mode.
- `PRIMARY` filenames never include `-<color>`.
- Sized `PRIMARY` filenames use `-<size>` only.
- `ALL` filenames include `-<color>` before the extension.
- `PRIMARY_COLOR` must exist in the approved palette.

The numbered step files define the authoritative palette, filenames, dimensions, counts, artwork constraints, and validation requirements.

## Output Directory Rules

When `GENERATION_MODE = PRIMARY`:

- Use the corresponding `step<n>` directory by default.
- An explicitly supplied output directory overrides the default.

When `GENERATION_MODE = ALL`:

- Require an explicit output directory before generation begins.
- Do not use a default `step<n>` directory.
- Do not infer or invent an output path.

## Approved Stage Directories

The approved artwork stages are:

| Step | Directory |
| --- | --- |
| 1 | `D:\source\repos\msixcli\images\graphic-design\step1` |
| 2 | `D:\source\repos\msixcli\images\graphic-design\step2` |
| 3 | `D:\source\repos\msixcli\images\graphic-design\step3` |
| 4 | `D:\source\repos\msixcli\images\graphic-design\step4` |
| 5 | `D:\source\repos\msixcli\images\graphic-design\step5` |

## Validation Rule

AI-generated artwork is untrusted until validated.

For every step:

1. Generate the required artifacts.
2. Run every validation specified by that step.
3. Correct any failure.
4. Repeat generation and validation until all requirements pass.
5. Proceed to the next numbered step only after the current step is approved.

Never treat generation alone as completion.
