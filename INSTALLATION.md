# Normal Windows 3.0 Setup installation

**These instructions apply to a complete locally assembled package.** This
public source prerelease does not contain the compiled driver, companion or
artwork, so extracting it alone is not sufficient to install the driver.

The original DEB resident must be loaded, and Windows 3.0 must already work
with a stock display driver. Initial target: M28 / PC 6310, DOS 6.22,
Windows real mode. Keep a known-working configuration for recovery.

## Install a complete package

1. Place all installer files together under `C:\EGCTEST\WIN16` and exit Windows.
2. From the installed Windows directory run
   `CALL C:\EGCTEST\WIN16\BACKUP.BAT`. Confirm that EGCBAK contains
   SYSTEM.INI, WIN.INI, WIN.COM and COMPLETE.TAG. Existing backups are retained;
   save a separate current copy if the retained backup is older.
3. Run Windows DOS `SETUP`, choose Display, then Other / manufacturer disk.
   Enter `C:\EGCTEST\WIN16` and select the EGC 640x400 16-colour candidate.
4. At a Windows-font media prompt, enter the directory containing the
   requested Windows 3.0 file. Examples: `C:\WINFILES\300`, `C:\WIN30`,
   `C:\WINDOWS\SYSTEM` or `A:\`. A floppy is optional. Use matching
   Windows 3.0 files for your installation; do not substitute Windows 3.1
   files by filename alone. Setup expands original compressed `.FON` files.
5. If asked for the EGC disk again, return to `C:\EGCTEST\WIN16`.
   Supply the Windows source again when HELVB.FON is requested. Setup does
   not search subdirectories automatically. Select Complete Changes again
   if it returns to the configuration screen after copying fonts.
6. Reboot with the resident loaded and use `WIN /R`. Check startup, text,
   controls, pointer and normal exit. Standard mode needs separate qualification.

Setup selects VGASYS.FON, VGAFIX.FON and EGAOEM.FON and registers HELVB.FON.
It also installs the text companion and assembles the startup components
into WIN.COM. Copying a display DLL alone does not perform those steps.

## Recovery

From DOS, change to the Windows directory and run:

```text
CALL C:\EGCTEST\WIN16\RESTORE.BAT
```

The helper restores SYSTEM.INI, WIN.INI and WIN.COM together and keeps the
backup. It requires CMPINI.COM to verify the copies. For another installer
location, pass the helper's actual path, for example
`CALL A:\RESTORE.BAT A:\CMPINI.COM`.
