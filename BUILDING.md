# Building from source

The release provides compiled DOS installation files; building is optional.
To build them yourself, use the corresponding EGC30SRC.ZIP or this repository.

## Tools and external inputs

Install Python 3, DOSBox-X and NASM on the host computer. Supply the historical
Windows 3.0 assembly/link/resource tools, headers and resource inputs listed
in BUILD-INPUTS.json under their applicable terms. That manifest specifies
relative filenames, sizes and SHA-256 hashes under BIN, INCLUDE and RES.
HELPER-INPUTS.json specifies the helper tools and expected output hashes.
The tools, headers, executable stub and SDK/DDK resource input files are not
bundled with the source.

Run the commands below from the repository root. Replace each uppercase path
placeholder with a path on your computer. Output directories must not already
exist. All compilation takes place inside those new directories.

## Display driver

```text
python build_driver.py --inputs INPUT_DIRECTORY --dosbox DOSBOX_EXECUTABLE --out DISPLAY_OUTPUT
```

The helper verifies inputs, runs the historical DOS build and checks the
result against BUILD-INPUTS.json. The display binary is written as
`DISPLAY_OUTPUT/OUT/EGCBASE.DRV`; it is named EGCORDER.DRV in the installer.
Logs are stored under the output directory's LOGS subdirectory.

Paths used inside the DOS batch file are paths in DOSBox's mounted build
directory. They are independent of the host computer's drive layout.

## Startup loaders and utilities

```text
python build_helpers.py --inputs INPUT_DIRECTORY --dosbox DOSBOX_EXECUTABLE --nasm NASM_EXECUTABLE --out HELPER_OUTPUT
```

This builds and verifies:

| Output | Source |
| --- | --- |
| EGCTEXT.GR2 | helpers/EGCTEXT.ASM |
| EGCLOGO.LGO | helpers/LOGO.ASM |
| EGCLOGN.LGO | helpers/LOGONO.ASM |
| CMPINI.COM | helpers/CMPINI.ASM |
| M28W3.COM | compat/M28W3/M28W3.ASM and its matching contexts |

The companion and startup loaders use MASM/LINK. The comparison utility and
M28 patcher use NASM. Outputs are placed under HELPER_OUTPUT/OUT.
The two startup loaders must be paired with their matching artwork streams
in assets/. Changing an artwork stream requires updating and rebuilding its
loader's payload-length definition as well.

## Installer archive

```text
python package.py --driver DISPLAY_OUTPUT/OUT/EGCBASE.DRV --helpers HELPER_OUTPUT/OUT --out EGC30A2.ZIP
```

The packager verifies the compiled-file hashes, includes the Setup files,
backup utilities, M28 instructions, both artwork variants and GPLv2 licence,
and checks the resulting ZIP. It does not include Windows font files.

The build scripts require exact release output hashes. For modified builds,
review and update the manifests together with the relevant code and tests.
An output hash alone is not a functional or hardware test. Distribute the
corresponding source with modified binaries under GPLv2.
