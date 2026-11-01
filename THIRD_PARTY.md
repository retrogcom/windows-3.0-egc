# External inputs and licence scope

The MIT licence covers the original source and documentation in this
repository. It does not relicense the third-party inputs used to build or
run the driver.

| Input | Role | Distributed here? |
| --- | --- | --- |
| Microsoft Windows 3.0 | Target operating system and original Setup | No |
| VGASYS.FON, VGAFIX.FON, EGAOEM.FON, HELVB.FON | System, fixed, OEM and Helv raster fonts | No; Setup uses user-supplied files |
| Windows SDK/DDK tools and headers | Assembly, linking and resource compilation | No; required input hashes are supplied |
| Historical WINSTUB.EXE and DDK UI resources | Executable stub, cursors, icons and control bitmaps | No; required input hashes are supplied |
| Converted Windows startup artwork | Native EGC colour-startup payload | No |
| Original Olivetti/AT&T DEB resident | Hardware mode and palette services | No |
| DOSBox-X and NASM | Host-side development tools | No |

The local compiled candidate incorporates some of the inputs above. Public
redistribution permission for its startup artwork and embedded historical
resources has not been established. The first public prerelease therefore
contains source and installation metadata only. A ready-to-install binary
release requires appropriate permissions, replacement resources, or a tested
local assembly process that obtains those resources from the user's files.

Functional API declarations, resource identifiers and file references do not
include the referenced third-party binaries. No vendor endorsement is implied.
