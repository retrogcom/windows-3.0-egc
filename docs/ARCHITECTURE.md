# Rendering architecture

The driver is a native 286 Windows display DLL. It exposes the Windows 3.0
GDI entry points through `EGCBASE.DEF`. A fixed palette maps Windows colour
indices to the resident's EGC colour codes once. The screen descriptor
represents 640x400 pixels with an 80-byte row pitch and four colour planes.
Memory BITMAPs and the physical display follow separate validation paths.

| Modules | Responsibility |
| --- | --- |
| EGCBASE, ALIAS, PLANES, SCREEN, RANGE | Activation, mapped surfaces, bounds and ownership |
| FILL, NARROW, BORDER | Rectangle fills, narrow strips and FastBorder |
| BLTCORE, BLTMAP, BLTRECT, COPY, ROWCOPY | Raster operations, mapping and copy rows |
| BITCACHE, MAPCACHE | Validated reusable bitmap/mapping data |
| TEXTFAST, TXTCORE, TXTDRAW | Glyph composition and text strips |
| DIB* | Packed and compressed DIB conversion and colour mapping |
| OUTPUT, AXIS, PIXEL, SPANS | Lines, pixels and spans |
| CURSOR, CURDATA | Software pointer exclusion, save/restore and movement |
| TRACE, DRAWDIAG | Lifecycle and drawing diagnostics |

Activation probes the resident, records the entry state, allocates/maps
resources and installs the palette before publishing an active screen.
Failure and teardown paths retain ownership information and attempt to
restore DOS text. Protected-mode bridges support Windows Standard mode;
the memory manager must provide a compatible extended-memory environment.

Optimisations target GDI primitives shared by Windows applications. Broad
fills use small row bands across all planes; narrow columns keep their faster
schedule. Copy loops select specialised row paths when their geometry and
bounds satisfy the required checks. Text uses bounded composition strips;
bitmap caches validate content rather than trusting a reusable segment alone.
Cursor exclusion protects intersecting draws. Rendering uses CPU access to
the planar display memory.

The colour startup runs before display-driver activation, through separate
WIN.COM components. `helpers/LOGO.ASM` handles the native EGC startup and
restoration contract; it is not itself the artwork. The desktop accepts that
mode handoff as well as the supported stock entry states.
