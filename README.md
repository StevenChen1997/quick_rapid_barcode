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
- `--scale <n>` pixel scale (default 4)
- `--height <n>` 1D height in modules (default 60)
- `--quiet <n>` 1D quiet zone in modules (default 10)
- `--cuda` use CUDA for encode/render (requires `make USE_CUDA=1`)
- `--sse` use SSE for encode/render (requires `make USE_SSE=1`)

## Notes

- QR encoder is version 1-L only (up to 17 bytes).
- Code39 supports: A-Z, 0-9, space, `-`, `.`.
- JPEG output requires libjpeg and `make USE_LIBJPEG=1`.
