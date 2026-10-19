# Third-party notices

QR version geometry and ECC-L block tables in `src/encoder_qr_common.h` are
adapted from Project Nayuki's QR Code generator:
https://github.com/nayuki/QR-Code-generator/blob/master/cpp/qrcodegen.cpp

Copyright (c) Project Nayuki. (MIT License)
https://www.nayuki.io/page/qr-code-generator-library

Permission is hereby granted, free of charge, to any person obtaining a copy of
this software and associated documentation files (the "Software"), to deal in
the Software without restriction, including without limitation the rights to
use, copy, modify, merge, publish, distribute, sublicense, and/or sell copies of
the Software, and to permit persons to whom the Software is furnished to do so,
subject to the following conditions:

- The above copyright notice and this permission notice shall be included in
  all copies or substantial portions of the Software.
- The Software is provided "as is", without warranty of any kind, express or
  implied, including but not limited to the warranties of merchantability,
  fitness for a particular purpose and noninfringement. In no event shall the
  authors or copyright holders be liable for any claim, damages or other
  liability, whether in an action of contract, tort or otherwise, arising from,
  out of or in connection with the Software or the use or other dealings in the
  Software.


## ZXing-C++ 2.3.0

Vendored core sources in `third_party/zxing-cpp` are used for PDF417 encoding and
barcode decoding. Upstream: https://github.com/zxing-cpp/zxing-cpp/tree/v2.3.0

License: Apache-2.0, with per-file copyright notices retained. The complete
license is in `third_party/zxing-cpp/LICENSE`. The library is compiled with the
legacy C++ writers; no Zint source or runtime dependency is included.

## stb_image

`third_party/stb_image.h` provides PNG/JPEG input. Upstream:
https://github.com/nothings/stb

Used under its MIT license, included with its copyright notice in the header.
The exact commit and SHA-256 are recorded in `third_party/SOURCES.json`.
