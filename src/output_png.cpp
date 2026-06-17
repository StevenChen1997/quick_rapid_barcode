#include "output_png.h"

#include <algorithm>
#include <fstream>
#include <vector>

static uint32_t crc32(const uint8_t *data, size_t len) {
    uint32_t crc = 0xFFFFFFFFu;
    for (size_t i = 0; i < len; ++i) {
        crc ^= data[i];
        for (int j = 0; j < 8; ++j) {
            crc = (crc >> 1) ^ (0xEDB88320u & (-(int)(crc & 1)));
        }
    }
    return ~crc;
}

static uint32_t adler32(const uint8_t *data, size_t len) {
    const uint32_t MOD = 65521;
    uint32_t a = 1, b = 0;
    for (size_t i = 0; i < len; ++i) {
        a = (a + data[i]) % MOD;
        b = (b + a) % MOD;
    }
    return (b << 16) | a;
}

static void write_u32(std::ofstream &out, uint32_t v) {
    out.put(static_cast<char>((v >> 24) & 0xFF));
    out.put(static_cast<char>((v >> 16) & 0xFF));
    out.put(static_cast<char>((v >> 8) & 0xFF));
    out.put(static_cast<char>(v & 0xFF));
}

static void write_chunk(std::ofstream &out, const char *type, const std::vector<uint8_t> &data) {
    write_u32(out, static_cast<uint32_t>(data.size()));
    out.write(type, 4);
    if (!data.empty()) {
        out.write(reinterpret_cast<const char *>(data.data()),
                  static_cast<std::streamsize>(data.size()));
    }
    std::vector<uint8_t> crcbuf;
    crcbuf.insert(crcbuf.end(), type, type + 4);
    crcbuf.insert(crcbuf.end(), data.begin(), data.end());
    uint32_t crc = crc32(crcbuf.data(), crcbuf.size());
    write_u32(out, crc);
}

bool write_png(const std::string &path, const Bitmap &bmp, std::string &err) {
    std::ofstream out(path, std::ios::binary);
    if (!out) {
        err = "Failed to open output file";
        return false;
    }
    const uint8_t sig[8] = {137, 80, 78, 71, 13, 10, 26, 10};
    out.write(reinterpret_cast<const char *>(sig), 8);

    std::vector<uint8_t> ihdr(13, 0);
    ihdr[0] = (bmp.w >> 24) & 0xFF;
    ihdr[1] = (bmp.w >> 16) & 0xFF;
    ihdr[2] = (bmp.w >> 8) & 0xFF;
    ihdr[3] = bmp.w & 0xFF;
    ihdr[4] = (bmp.h >> 24) & 0xFF;
    ihdr[5] = (bmp.h >> 16) & 0xFF;
    ihdr[6] = (bmp.h >> 8) & 0xFF;
    ihdr[7] = bmp.h & 0xFF;
    ihdr[8] = 8;  // bit depth
    ihdr[9] = 0;  // grayscale
    ihdr[10] = 0; // compression
    ihdr[11] = 0; // filter
    ihdr[12] = 0; // interlace
    write_chunk(out, "IHDR", ihdr);

    std::vector<uint8_t> raw;
    raw.reserve(static_cast<size_t>((bmp.w + 1) * bmp.h));
    for (int y = 0; y < bmp.h; ++y) {
        raw.push_back(0);
        const uint8_t *row = &bmp.pixels[static_cast<size_t>(y * bmp.w)];
        raw.insert(raw.end(), row, row + bmp.w);
    }

    std::vector<uint8_t> zlib;
    zlib.push_back(0x78);
    zlib.push_back(0x01);

    size_t pos = 0;
    while (pos < raw.size()) {
        size_t blockLen = std::min<size_t>(65535, raw.size() - pos);
        bool final = (pos + blockLen) == raw.size();
        zlib.push_back(final ? 0x01 : 0x00);
        uint16_t len = static_cast<uint16_t>(blockLen);
        uint16_t nlen = static_cast<uint16_t>(~len);
        zlib.push_back(static_cast<uint8_t>(len & 0xFF));
        zlib.push_back(static_cast<uint8_t>((len >> 8) & 0xFF));
        zlib.push_back(static_cast<uint8_t>(nlen & 0xFF));
        zlib.push_back(static_cast<uint8_t>((nlen >> 8) & 0xFF));
        zlib.insert(zlib.end(), raw.begin() + static_cast<long>(pos),
                    raw.begin() + static_cast<long>(pos + blockLen));
        pos += blockLen;
    }

    uint32_t adler = adler32(raw.data(), raw.size());
    zlib.push_back(static_cast<uint8_t>((adler >> 24) & 0xFF));
    zlib.push_back(static_cast<uint8_t>((adler >> 16) & 0xFF));
    zlib.push_back(static_cast<uint8_t>((adler >> 8) & 0xFF));
    zlib.push_back(static_cast<uint8_t>(adler & 0xFF));

    write_chunk(out, "IDAT", zlib);
    write_chunk(out, "IEND", {});
    return true;
}
