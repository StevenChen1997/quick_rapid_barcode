# quick_rapid_barcode

A minimal 1D/2D barcode generator in C++ (CLI).

This project is assisted by AI and vibe coding tools.

## Build

```bash
make
```

JPEG output (optional, requires libjpeg):

```bash
make USE_LIBJPEG=1
```

CUDA build (optional, requires nvcc toolchain):

```bash
make USE_CUDA=1
```

SSE build (optional, x86 with SSE2 support):

```bash
make USE_SSE=1
```

## Usage

```bash
./qrb -t "abcdefg1234567" -s code128
./qrb -t "590123412345" -s ean13
./qrb -t "01234567890" -s upc
./qrb -t "HELLO-39" -s code39
./qrb -t "123456" -s itf
./qrb -t "HELLO" -s qr -o out.png -f png
./qrb -t "HELLO" -s qr -o out.png -f png --cuda
./qrb -t "HELLO" -s qr -o out.png -f png --sse
```

### Common options

- `-t <text>` input text
- `-s <symbology>`: `ean13 | upc | code128 | code39 | itf | qr`
- `-o <file>` output image file
- `-f <format>`: `ascii | png | jpg`
- `--scale <n>` image pixel scale (default 4)
- `--qr-version <n>` exact QR version 1-40, or 0 for automatic selection (default 0)
- `--terminal-scale <n>` terminal magnification 1-8 (default 1)
- `--height <n>` 1D height in modules (default 5)
- `--quiet <n>` 1D quiet zone in modules (default 10)
- `--cuda` use CUDA for encode/render (requires `make USE_CUDA=1`)
- `--sse` use SSE for encode/render (requires `make USE_SSE=1`)

## Notes

- QR byte-mode encoder supports versions 1-40 at ECC L (up to 2953 bytes).
  It automatically selects the smallest version that fits. UTF-8 characters may
  occupy several bytes. Mask 0 is retained across CPU, SSE, and CUDA paths.
- Code39 supports: A-Z, 0-9, space, `-`, `.`.
- JPEG output requires libjpeg and `make USE_LIBJPEG=1`.

## QR output in WSL / Windows terminals

QR terminal output defaults to `--terminal auto`:

- In an interactive WSL terminal outside VSCode, use ANSI background colors and
  two spaces per module. This avoids Unicode block glyphs and their font/encoding
  differences. This mode uses full rows; `--half` does not affect it.
- In the VSCode terminal, retain compact Unicode half-height output. Interactive
  output sets black foreground and white background explicitly.
- Redirected auto output uses Unicode without ANSI color sequences.

You can select either renderer explicitly (run these commands inside WSL):

```bash
./qrb -t "https://example.com" -s qr --terminal ansi
./qrb -t "https://example.com" -s qr --terminal unicode
```

`ansi` requires a terminal supporting ANSI/VT background colors, such as Windows
Terminal. It also emits color sequences when explicitly selected for redirected
output. See Microsoft's [console VT documentation](https://learn.microsoft.com/en-us/windows/console/console-virtual-terminal-sequences).
The Unicode mode requires UTF-8 and a monospace font with block characters.
If Unicode blocks look broken, use `--terminal ansi`.

Both modes add a four-module quiet zone. Terminal width is read from the stdout
TTY, with `COLUMNS` as a fallback. If the complete QR does not fit, QRB reports the
required column count and exits with an error without printing a partial QR.
Widen the terminal or reduce its font size, choose compact `unicode` mode when
its glyphs render correctly, or write a PNG:

```bash
./qrb -t "https://example.com" -s qr -o qr.png
```

`--hscale` must be 1 for QR terminal output. Earlier horizontal merging and
cropping discarded QR data and could make the result unscannable.

Terminal regression checks (Python 3, Linux/WSL):

```bash
make
python3 tests/test_terminal.py
```

These checks compare both terminal renderings against PNG modules and exercise
pseudo-terminal width, WSL/VSCode mode selection, redirection, and error handling.
They do not test Windows font rendering or camera scanning.

## Larger QR codes

Capacity and display size are separate controls:

- `--qr-version 0` chooses the smallest QR that can hold the input. An explicit
  version from 1 through 40 fixes the module count; oversized input is rejected.
- `--terminal-scale 2` doubles both dimensions of every module and the quiet zone,
  without changing data. ANSI remains glyph-free; Unicode retains half-height
  packing. Width checks include magnification.
- `--scale 8` sets image pixels per module and has no effect on terminal output.

```bash
# Auto-select a larger version for a long URL.
./qrb -t "https://example.com/search?category=aircraft&manufacturer=Boeing&model=747-400&registration=B-18361" -s qr
# Request more modules even for short data (requires at least 91 ANSI columns).
./qrb -t "https://example.com" -s qr --qr-version 5 --terminal ansi
# Double the terminal size (this URL requires at least 133 ANSI columns).
./qrb -t "https://example.com" -s qr --terminal-scale 2
# Version 5 image: (37 + 8 quiet-zone modules) * 8 = 360 pixels per side.
./qrb -t "https://example.com" -s qr --qr-version 5 --scale 8 -o qr-large.png
```

A version V QR has `17 + 4*V` modules per side. With the four-module quiet zone,
ANSI output needs `2 * (25 + 4*V) * terminal-scale` columns and
`(25 + 4*V) * terminal-scale` rows, plus a spare column to prevent autowrap.
Unicode half-height uses half as many columns and approximately half as many
rows. Large codes may require a wider/taller window, smaller terminal font, or
PNG output. A larger version does not itself increase the error-correction level.

Optional independent encoding/scanning checks require Python packages `qrcode`,
`pillow`, and `zxing-cpp` in your test environment:

```bash
python3 tests/test_qr_expansion.py
# After building with make -B USE_SSE=1:
QRB_TEST_SSE=1 python3 tests/test_qr_expansion.py
```

These tests compare all 40 versions at byte capacity against an independent
encoder, decode the images with ZXing, and check overflow, auto version selection,
UTF-8 URLs, and invalid arguments. The normal terminal tests also check integer
magnification. QR version tables/geometry attribution is in
[THIRD_PARTY_NOTICES.md](THIRD_PARTY_NOTICES.md).
