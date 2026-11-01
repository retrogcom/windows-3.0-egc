# Windows 3.0 EGC display driver

A 640x400, 16-colour Windows 3.0 display driver for the **Olivetti EGC / AT&T
Display Enhancement Board (DEB)**, developed for the Olivetti M28 / AT&T
PC 6310 with an 8 MHz 80286.

**Source prerelease: 0.21-appearance.1.** This repository contains original
driver and helper sources, build-input hashes and OEM Setup templates.
There is no ready-to-install driver download in this prerelease. The local
compiled candidate contains separately licensed Windows/DDK resources and
startup artwork, so it is not included pending redistribution review or
replacement. See [third-party inputs](THIRD_PARTY.md).

## What it does

- Implements Windows GDI drawing in sixteen fixed colours: fills, borders,
  lines, pixels, text, bitmap copies, DIB conversion and software cursors.
- Uses shared rendering paths rather than application-specific acceleration.
- Supports native colour-startup handoff and normal restoration to DOS text.
- Draws broad filled areas in four-row bands across the colour planes, to
  reduce the visible effect of drawing widely separated rows first.
- Provides normal Windows DOS Setup metadata, including fonts obtained from
  the user's Windows 3.0 files. A hard-drive source folder works; A: is optional.

The hardware baseline has been tested on an M28 using DOS 6.22 and Windows
3.0 real mode. This later appearance candidate has desk validation and still
needs its own M28 startup/fill/performance comparison. Standard mode requires
separate target qualification; 386 Enhanced mode and Windows 3.1 are unsupported
by this release. The original DEB resident driver must already be loaded.

## Build and installation

Read [BUILDING.md](BUILDING.md). The display build requires Python 3,
DOSBox-X and the separately supplied historical compiler/header/resource
inputs listed in [BUILD-INPUTS.json](BUILD-INPUTS.json). The helper verifies
those inputs and requires a byte-identical display binary.

[INSTALLATION.md](INSTALLATION.md) documents the tested OEM Setup procedure
for a complete locally assembled package. The templates in `installer/`
are not independently installable: compiled driver/helper files and startup
artwork are not in this source release. Windows, fonts, DEB resident software,
SDK/DDK toolchains, commercial games and disk images are not distributed here.

## Project layout

| Path | Contents |
| --- | --- |
| `driver/SRC/` | 286 assembly display implementation and resource/link definitions |
| `driver/BUILD.BAT` | Historical DOS build commands |
| `helpers/` | Native startup loader, text companion and backup comparison utility sources |
| `installer/` | OEMSETUP.INF and backup/restore scripts |
| `build_driver.py` | Isolated, input-checked display build |
| `docs/` | Architecture and validation summary |
| `SOURCE-MANIFEST.json` | SHA-256 hashes of the exported files |

The display filename is `EGCORDER.DRV`; its diagnostic banner still says
0.21. Identify this candidate by its hash:

```text
a6176ac56dc28e992321b7986761cdd8fa955277bfe296c9587817859810d58b
```

## Testing and reports

See [validation](docs/VALIDATION.md) for completed checks and their limits.
Report the driver hash, DOS/Windows versions, real or Standard mode, resident
version and the operation involved. Include a photo for visual defects and
relevant `EGC16.LOG` lines after exiting Windows normally. Remove personal
paths or other private details from logs before posting. Use the same
benchmark executable and configuration for performance comparisons.

## Licence and scope

Original code and documentation: **MIT**, copyright (c) 2026 **retrogcom**.
See [LICENSE](LICENSE). External Microsoft/Bitstream/Olivetti/AT&T software
retains its own terms. This is an independent project, not a vendor release.

This repository covers the Windows 3.0 driver only. Other EGC projects,
including game ports and Windows 2 work, have separate scope and licensing.
