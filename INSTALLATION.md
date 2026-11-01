# Installation

These instructions use `<WINDOWS>` for your installed Windows directory and
`<DRIVER>` for the extracted WIN16 directory. Replace each placeholder with
its actual drive and directory; do not type the angle brackets. Use DOS 8.3
names without spaces. Run the commands at the DOS prompt, outside Windows.

## 1. Prepare

1. Install Windows 3.0 and confirm it works with a stock display driver.
2. Load the original Olivetti/AT&T DEB resident driver using its normal DOS
   configuration. The EGC driver does not install the resident for you.
3. Extract EGC30A2.ZIP and copy the complete WIN16 directory to a DOS-accessible
   disk. Keep its files together.
4. Have your matching Windows 3.0 installation files available. Fonts are
   not included in the package. A directory on a hard disk is sufficient;
   a floppy drive is optional.

## 2. Back up Windows

Select the drive containing Windows, then change to its directory:

```text
CD <WINDOWS>
<DRIVER>\BACKUP.BAT <DRIVER>\CMPINI.COM
```

Confirm the backup completed. EGCBAK must contain SYSTEM.INI, WIN.INI,
WIN.COM and COMPLETE.TAG. An existing backup is retained; save another copy
of your current configuration if that retained backup belongs to an older
installation. Do not install after a backup error.

## 3. Install the display driver

From the Windows directory, run:

```text
SETUP
```

Choose **Display**, then **Other / manufacturer-supplied disk**. Enter the
path to `<DRIVER>` and select either:

- **Olivetti EGC 640x400 16 colors (English startup)**
- **Olivetti EGC 640x400 16 colors (Norwegian startup)**

At a Windows-font prompt, enter the directory containing the requested
VGASYS.FON, VGAFIX.FON, EGAOEM.FON or HELVB.FON. Installed fonts can also be
used. Setup reads the original compressed Windows 3.0 `.FON` files directly;
no manual decompression is needed. It does not search subdirectories.
Use the files for your Windows 3.0 edition and language.

If Setup asks for the EGC disk again, enter `<DRIVER>` again. Supply the
Windows font directory again if requested. Select **Complete Changes**
again if Setup returns to the configuration screen after copying fonts.

Setup installs the display and text companion, selects/registers the fonts
and assembles the selected colour-startup components into WIN.COM. Changing
only SYSTEM.INI or copying only the display DLL does not complete installation.

## 4. M28 Standard-mode compatibility

Before running Standard mode on an M28 with BIOS 1.08, follow
[M28.md](docs/M28.md). The included M28W3 utility fixes the DOSX BIOS return
frame and protects the EGC video query used during colour startup.

**Do not use the combined patch on BIOS 2.12.** The patcher does not check the
BIOS version. Real-mode Windows does not use DOSX.EXE.

## 5. Start Windows

Reboot with the DEB resident and your usual memory configuration loaded.
From the Windows directory:

```text
WIN /R
```

For Standard mode, after the required M28 compatibility steps:

```text
WIN /S
```

DOS 3.1 is tested in both modes on the M28. Confirm that the colour startup,
desktop, fonts and mouse look correct and that exiting returns to readable
DOS text.

## Recovery

From DOS, select the Windows drive and directory, then run:

```text
<DRIVER>\RESTORE.BAT <DRIVER>\CMPINI.COM
```

This restores SYSTEM.INI, WIN.INI and WIN.COM together and retains EGCBAK.
The helper checks each restored file against its backup. Restoring the display
configuration does not undo M28W3; its separate DOSX recovery steps are in
[M28.md](docs/M28.md).
