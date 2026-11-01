# Validation and limitations

Source snapshot: 0.21-appearance.1. The numeric runtime banner remains 0.21.
Expected display SHA-256:
`a6176ac56dc28e992321b7986761cdd8fa955277bfe296c9587817859810d58b`.

Recorded checks for this display candidate include:

- A byte-identical isolated rebuild using the specified historical inputs.
- All 37 native driver suites.
- 1,542 independent framebuffer/write-order cases, 1,024 additional solid-edge
  cases and 13 mode-42 activation/recovery cases.
- 128 primitive probes, 1,006 captured graphics calls and 199 popup calls.
- Original Windows startup, four reference-matching rendered buffers and
  normal DOS text restoration in a CPU-aperture fixture.

Packaging checks on 1 November 2026 used original Windows 3.0 DOS Setup in
DOSBox-X. They covered English and localized compressed font files in a
hard-drive folder, a floppy font source and existing installed fonts. Font
expansion matched an independent decoder. System/font selections, HELVB
registration, preservation of unrelated settings, colour WIN.COM composition
and exact configuration/startup rollback passed.

These are recorded research results, not a bundled public test harness.
Windows fixtures and private logs are excluded from the repository. The
CPU-aperture fixture is not EGC scanout emulation. Instruction/clock-model
results exclude important physical timing effects and are not M28 benchmarks.
Font registration checks do not prove live Program Manager font selection.

The underlying 0.21 driver has M28 hardware feedback. This later appearance
candidate still needs a physical comparison of colour startup, progressive
fills and performance. The new fill schedule trades some CPU cost for drawing
order: the representative 256x32 solid-fill model cost is 10.8% above the
preceding bank-ordered candidate; broad logical fills are within 1.6%.
Thin columns retain their earlier schedule. These are model comparisons,
not universal or measured hardware slowdowns.

For the next short hardware comparison, retain the same W3BENCH executable
on both drivers and compare 02 solid fill, 03 pattern fill, 04 paired inversion
and 2E dialog. Watch startup, partial edges, colour flashes, pointer trails
and exit to readable DOS text. Standard-mode hardware testing is separate.
