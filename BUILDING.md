# Building the display driver

This source release does not bundle the historical toolchain, SDK/DDK
headers, executable stub or UI artwork resources. Supply those inputs
separately under their applicable terms. `BUILD-INPUTS.json` records exact
relative filenames, byte counts and SHA-256 hashes; `driver/tool-inputs.json`
and `driver/resource-inputs.json` record provenance. The recorded `work/`
paths identify the research source locations, not downloadable directories.

## Exact display rebuild

1. Install Python 3 and DOSBox-X on the host computer.
2. Prepare an input directory containing the `BIN`, `INCLUDE` and `RES`
   trees listed in `BUILD-INPUTS.json`. Do not substitute files from a
   different SDK/DDK revision silently.
3. From this repository run, adjusting the three host paths:

```text
python build_driver.py --inputs PATH_TO_INPUT_TREE --dosbox PATH_TO_DOSBOX_X --out NEW_BUILD_DIRECTORY
```

The output directory must not already exist. The script checks every input,
creates an isolated DOS build tree and runs `driver/BUILD.BAT`. It then checks
`OUT/EGCBASE.DRV` against the expected SHA-256. Build logs are under `LOGS/`.
The `C:` paths in the assembly and batch file are guest paths inside this
isolated tree; they do not refer to the host computer's C: root.

Expected output SHA-256:

```text
a6176ac56dc28e992321b7986761cdd8fa955277bfe296c9587817859810d58b
```

This exact source/input combination has passed a byte-identical rebuild.
The helper deliberately rejects modified output; when developing changes,
review and update the expected-output manifest as part of that change.
Do not treat merely updating a hash as a correctness test.

## Companion sources and package limits

`helpers/EGCTEXT.ASM` and `helpers/LOGO.ASM` use the historical MASM syntax.
`helpers/CMPINI.ASM` uses NASM syntax. `build_driver.py` builds only the display
DLL; it does not build these helper components or assemble an installer.
The native logo loader has a fixed 13,291-byte payload contract; changing its
artwork requires rebuilding and validating both loader and payload.

The OEM Setup template expects EGCORDER.DRV, EGCTEXT.GR2, EGCLOGO.LGO,
EGCLOGO.RLE, EGCPRES.TAG and the backup helper CMPINI.COM. These compiled
files are not in this public source release. Fonts are requested from the
user's Windows 3.0 files. Do not present the template alone as an installer.

The full native regression and original-Windows replay harnesses depend on
private Windows fixtures and are not included. Published validation results
describe completed checks; this repository does not claim to include an EGC
hardware emulator or the full test environment.
