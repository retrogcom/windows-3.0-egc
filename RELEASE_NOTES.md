# 0.21-appearance.1 ? source prerelease

Initial public source release of the Olivetti EGC / AT&T DEB 640x400,
16-colour Windows 3.0 driver, under the MIT licence for original code.

Includes the 286 renderer, shared fill/copy/text/cursor optimisations, the
native colour-startup loader source, four-row area-fill scheduling, OEM Setup
metadata with user-supplied Windows fonts, build instructions and exact input
hashes. The display sources are unchanged from the byte-identically rebuilt
appearance candidate; its diagnostic banner still reports 0.21.

**This is a source release, not an installable driver package.** Windows fonts,
SDK/DDK binaries/resources, startup artwork, the DEB resident and disk images
are excluded. The compiled installer remains unpublished while artwork and
embedded-resource redistribution is resolved. See THIRD_PARTY.md and BUILDING.md.

The 0.21 base has M28 feedback. This later candidate still needs hardware
qualification for the appearance/performance tradeoff. No new hardware
benchmark, Standard-mode qualification or Windows 3.1 support is claimed.
