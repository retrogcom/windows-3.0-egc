# Startup artwork

EGCLOGO.RLE contains the English Windows 3.0 startup artwork; EGCLOGN.RLE
contains the Norwegian artwork. Each stream represents four native EGC
planes at 640x400 and is paired with the same-named LGO loader.

These use a native byte-packet format, not Microsoft BMP RLE4. The four-byte
header is EGC1. For each packet, a control byte below 128 is followed by
control+1 literal bytes; a control byte at least 128 is followed by one byte
repeated control-127 times. Decoding produces four consecutive 32 KiB planes.
Each plane stores four interleaved row banks with an 80-byte visible row pitch.

The images preserve the complete original 640x480 artwork at the EGC's
640x400 display size. Preview images are under docs/images/. Microsoft
copyright notices remain part of the artwork; see THIRD_PARTY.md.
