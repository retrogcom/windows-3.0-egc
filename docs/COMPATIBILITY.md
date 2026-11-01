# Compatibility and troubleshooting

## Supported configuration

The target is an Olivetti EGC / AT&T DEB at 640x400 in sixteen colours,
using Windows 3.0 on a 286 or later CPU. The Olivetti M28 / AT&T PC 6310 has
been tested with DOS 3.1 in both real mode and Standard mode. DOS 6.22 has
also been used. Windows 386 Enhanced mode and Windows 3.1 are not supported.

Load the original DEB resident before Windows. Standard mode also needs a
working extended-memory configuration; the display package does not install
or configure a RAM-card driver or memory manager.

The bundled M28 DOSX patch is for BIOS 1.08 and must not be applied to BIOS
2.12. Its `/C` option checks the executable, not the BIOS. See [M28.md](M28.md).

## Fonts or Setup media prompts

Supply the requested font from the matching Windows 3.0 installation files.
A hard-drive folder works. Point Setup at the directory containing the file,
not its parent. Compressed Windows 3.0 `.FON` files are accepted directly.
If asked for the EGC disk, return to the extracted WIN16 directory.

## Startup screen remains monochrome

Run Windows DOS Setup to install the complete EGC entry. The startup screen
is assembled into WIN.COM and is not changed by replacing the display DLL
alone. English and Norwegian colour artwork are separate Setup choices.

## Standard mode fails during startup

Confirm the resident is loaded, the normal extended-memory setup works and
the BIOS version is supported by the M28 correction. On BIOS 1.08, check
both DOSX routines with M28W3 /C. Keep refusal messages and do not substitute
hard-coded patch offsets. Test real mode separately with WIN /R.

## Recovering a working desktop

Exit to DOS and use RESTORE.BAT with the full path to CMPINI.COM. This
restores the three saved Windows configuration/startup files. To undo a
DOSX correction, restore its separate DOSX.B13 backup as described in M28.md.

## Drawing and performance

The driver renders four colour planes on the CPU. Broad fills progress in
small row bands; drawing is not an atomic screen update. Performance depends
on the operation and machine. A software cursor is used. The text companion
supports text-mode switching; arbitrary DOS graphics switching is not covered.

When reporting a problem, include the release, computer, BIOS, DOS/Windows
versions, mode and steps to reproduce it. A photo is useful for visual faults.
Keep benchmark versions and settings the same for comparisons.
