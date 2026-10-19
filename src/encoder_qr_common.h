#pragma once

#include <algorithm>
#include <cstdint>
#include <cstdlib>
#include <string>
#include <vector>

#include "qrb_types.h"

namespace qr_internal {

struct QrSpec {
    int version;
    int size;
    int dataCodewords;
    int eccCodewords;
    int blocks;
};

// QR version geometry and ECC-L tables adapted from Project Nayuki (MIT).
// See THIRD_PARTY_NOTICES.md for attribution and license.
inline QrSpec qr_spec(int version) {
    static const int ecc[] = {
        0, 7, 10, 15, 20, 26, 18, 20, 24, 30, 18, 20, 24, 26, 30, 22,
        24, 28, 30, 28, 28, 28, 28, 30, 30, 26, 28, 30, 30, 30, 30,
        30, 30, 30, 30, 30, 30, 30, 30, 30, 30
    };
    static const int blocks[] = {
        0, 1, 1, 1, 1, 1, 2, 2, 2, 2, 4, 4, 4, 4, 4, 6,
        6, 6, 6, 7, 8, 8, 9, 9, 10, 12, 12, 12, 13, 14, 15,
        16, 17, 18, 19, 19, 20, 21, 22, 24, 25
    };
    int modules = (16 * version + 128) * version + 64;
    if (version >= 2) {
        int count = version / 7 + 2;
        modules -= (25 * count - 10) * count - 55;
    }
    if (version >= 7) {
        modules -= 36;
    }
    return {version, version * 4 + 17,
            modules / 8 - ecc[version] * blocks[version], ecc[version], blocks[version]};
}

struct QrBase {
    ModuleMatrix m;
    std::vector<uint8_t> isFunc;
    int size = 0;
};

inline void draw_finder(ModuleMatrix &m, std::vector<uint8_t> &isFunc, int x, int y) {
    for (int dy = -1; dy <= 7; ++dy) {
        for (int dx = -1; dx <= 7; ++dx) {
            int xx = x + dx;
            int yy = y + dy;
            if (xx < 0 || yy < 0 || xx >= m.w || yy >= m.h) {
                continue;
            }
            bool border = (dx == -1 || dx == 7 || dy == -1 || dy == 7);
            bool inner = (dx >= 2 && dx <= 4 && dy >= 2 && dy <= 4);
            bool outer = (dx >= 0 && dx <= 6 && dy >= 0 && dy <= 6
                          && (dx == 0 || dx == 6 || dy == 0 || dy == 6));
            uint8_t val = (outer || inner) ? 1 : 0;
            if (border) {
                val = 0;
            }
            m.set(xx, yy, val);
            isFunc[static_cast<size_t>(yy * m.w + xx)] = 1;
        }
    }
}

inline void draw_alignment(ModuleMatrix &m, std::vector<uint8_t> &isFunc, int cx, int cy) {
    for (int dy = -2; dy <= 2; ++dy) {
        for (int dx = -2; dx <= 2; ++dx) {
            int x = cx + dx;
            int y = cy + dy;
            if (x < 0 || y < 0 || x >= m.w || y >= m.h) {
                continue;
            }
            int dist = std::max(std::abs(dx), std::abs(dy));
            uint8_t val = (dist == 2 || dist == 0) ? 1 : 0;
            m.set(x, y, val);
            isFunc[static_cast<size_t>(y * m.w + x)] = 1;
        }
    }
}

static std::vector<uint8_t> gf_exp;
static std::vector<uint8_t> gf_log;

inline void gf_init() {
    if (!gf_exp.empty()) {
        return;
    }
    gf_exp.resize(512, 0);
    gf_log.resize(256, 0);
    uint8_t x = 1;
    for (int i = 0; i < 255; ++i) {
        gf_exp[i] = x;
        gf_log[x] = static_cast<uint8_t>(i);
        x = static_cast<uint8_t>(x << 1) ^ (x & 0x80 ? 0x1d : 0x00);
    }
    for (int i = 255; i < 512; ++i) {
        gf_exp[i] = gf_exp[i - 255];
    }
}

inline uint8_t gf_mul(uint8_t a, uint8_t b) {
    if (a == 0 || b == 0) {
        return 0;
    }
    return gf_exp[gf_log[a] + gf_log[b]];
}

inline std::vector<uint8_t> rs_generator(int degree) {
    std::vector<uint8_t> result(static_cast<size_t>(degree), 0);
    result[static_cast<size_t>(degree - 1)] = 1;
    uint8_t root = 1;
    for (int i = 0; i < degree; ++i) {
        for (int j = 0; j < degree; ++j) {
            result[static_cast<size_t>(j)] = gf_mul(result[static_cast<size_t>(j)], root);
            if (j + 1 < degree) {
                result[static_cast<size_t>(j)] ^= result[static_cast<size_t>(j + 1)];
            }
        }
        root = gf_mul(root, 2);
    }
    return result;
}

inline std::vector<uint8_t> rs_remainder(const std::vector<uint8_t> &data,
                                         const std::vector<uint8_t> &generator) {
    std::vector<uint8_t> result(generator.size(), 0);
    for (uint8_t b : data) {
        uint8_t factor = b ^ result[0];
        for (size_t i = 0; i + 1 < result.size(); ++i) {
            result[i] = static_cast<uint8_t>(result[i + 1] ^ gf_mul(generator[i], factor));
        }
        result.back() = gf_mul(generator.back(), factor);
    }
    return result;
}

inline int get_format_bits(int eccLevel, int mask) {
    int format = (eccLevel << 3) | mask;
    int data = format << 10;
    const int generator = 0x537;
    for (int i = 14; i >= 10; --i) {
        if ((data >> i) & 1) {
            data ^= generator << (i - 10);
        }
    }
    int bits = ((format << 10) | (data & 0x3FF)) ^ 0x5412;
    return bits & 0x7FFF;
}

inline void apply_mask_cpu(ModuleMatrix &m, const std::vector<uint8_t> &isFunc) {
    for (int y = 0; y < m.h; ++y) {
        for (int x = 0; x < m.w; ++x) {
            if (isFunc[static_cast<size_t>(y * m.w + x)]) {
                continue;
            }
            if (((x + y) & 1) == 0) {
                m.set(x, y, m.get(x, y) ^ 1);
            }
        }
    }
}

inline void apply_format_bits(ModuleMatrix &m, std::vector<uint8_t> &isFunc, int size) {
    int formatBits = get_format_bits(0x01, 0);
    auto set_fmt = [&](int x, int y, int bit) {
        m.set(x, y, bit ? 1 : 0);
        isFunc[static_cast<size_t>(y * size + x)] = 1;
    };
    for (int i = 0; i <= 5; ++i) {
        set_fmt(8, i, (formatBits >> i) & 1);
    }
    set_fmt(8, 7, (formatBits >> 6) & 1);
    set_fmt(8, 8, (formatBits >> 7) & 1);
    set_fmt(7, 8, (formatBits >> 8) & 1);
    for (int i = 9; i < 15; ++i) {
        set_fmt(14 - i, 8, (formatBits >> i) & 1);
    }

    for (int i = 0; i < 8; ++i) {
        set_fmt(size - 1 - i, 8, (formatBits >> i) & 1);
    }
    for (int i = 8; i < 15; ++i) {
        set_fmt(8, size - 15 + i, (formatBits >> i) & 1);
    }
}

inline bool build_qr_base(const std::string &input, QrBase &out, std::string &err,
                          int requested_version = 0) {
    if (requested_version < 0 || requested_version > 40) {
        err = "QR version must be 0 (auto) or 1-40";
        return false;
    }
    if (input.size() > 2953) {
        err = "QR byte mode at ECC L supports up to 2953 bytes (version 40)";
        return false;
    }
    QrSpec selected{};
    const int data_len = static_cast<int>(input.size());
    for (int v = requested_version ? requested_version : 1; v <= 40; ++v) {
        QrSpec candidate = qr_spec(v);
        int needed = 4 + (v < 10 ? 8 : 16) + data_len * 8;
        if (needed <= candidate.dataCodewords * 8) {
            selected = candidate;
            break;
        }
        if (requested_version) {
            break;
        }
    }
    if (!selected.version) {
        err = "Input does not fit requested QR version; use --qr-version 0 for auto";
        return false;
    }
    const QrSpec *spec = &selected;

    std::vector<int> bits;
    auto append_bits_n = [&](int val, int n) {
        for (int i = n - 1; i >= 0; --i) {
            bits.push_back((val >> i) & 1);
        }
    };

    append_bits_n(0x4, 4); // byte mode
    append_bits_n(data_len, spec->version < 10 ? 8 : 16);
    for (unsigned char c : input) {
        append_bits_n(c, 8);
    }
    int capacityBits = spec->dataCodewords * 8;
    int terminator = std::min(4, capacityBits - static_cast<int>(bits.size()));
    append_bits_n(0, terminator);
    while (bits.size() % 8 != 0) {
        bits.push_back(0);
    }

    std::vector<uint8_t> data;
    data.reserve(static_cast<size_t>(spec->dataCodewords));
    for (size_t i = 0; i < bits.size(); i += 8) {
        uint8_t b = 0;
        for (int j = 0; j < 8; ++j) {
            b = static_cast<uint8_t>((b << 1) | bits[i + j]);
        }
        data.push_back(b);
    }
    uint8_t padBytes[2] = {0xEC, 0x11};
    int padIdx = 0;
    while (static_cast<int>(data.size()) < spec->dataCodewords) {
        data.push_back(padBytes[padIdx % 2]);
        ++padIdx;
    }

    gf_init();
    std::vector<uint8_t> gen = rs_generator(spec->eccCodewords);
    // Split into short/long blocks, then interleave data and ECC separately.
    std::vector<std::vector<uint8_t>> blocks;
    std::vector<std::vector<uint8_t>> parity;
    int short_size = spec->dataCodewords / spec->blocks;
    int short_count = spec->blocks - spec->dataCodewords % spec->blocks;
    size_t offset = 0;
    for (int i = 0; i < spec->blocks; ++i) {
        int length = short_size + (i >= short_count ? 1 : 0);
        blocks.emplace_back(data.begin() + offset, data.begin() + offset + length);
        parity.push_back(rs_remainder(blocks.back(), gen));
        offset += length;
    }
    std::vector<uint8_t> all;
    for (int column = 0; column <= short_size; ++column) {
        for (const auto &block : blocks) {
            if (column < static_cast<int>(block.size())) {
                all.push_back(block[column]);
            }
        }
    }
    for (int column = 0; column < spec->eccCodewords; ++column) {
        for (const auto &block : parity) {
            all.push_back(block[column]);
        }
    }

    int size = spec->size;
    ModuleMatrix m(size, size, 0);
    std::vector<uint8_t> isFunc(static_cast<size_t>(size * size), 0);

    draw_finder(m, isFunc, 0, 0);
    draw_finder(m, isFunc, size - 7, 0);
    draw_finder(m, isFunc, 0, size - 7);

    for (int i = 8; i < size - 8; ++i) {
        m.set(i, 6, (i % 2 == 0) ? 1 : 0);
        m.set(6, i, (i % 2 == 0) ? 1 : 0);
        isFunc[static_cast<size_t>(6 * size + i)] = 1;
        isFunc[static_cast<size_t>(i * size + 6)] = 1;
    }

    if (spec->version >= 2) {
        int count = spec->version / 7 + 2;
        int step = (spec->version * 8 + count * 3 + 5) / (count * 4 - 4) * 2;
        std::vector<int> positions(count, 6);
        for (int i = count - 1; i >= 1; --i) {
            positions[i] = size - 7 - (count - 1 - i) * step;
        }
        for (int y = 0; y < count; ++y) {
            for (int x = 0; x < count; ++x) {
                if ((x == 0 && (y == 0 || y == count - 1))
                    || (y == 0 && x == count - 1)) {
                    continue;
                }
                draw_alignment(m, isFunc, positions[x], positions[y]);
            }
        }
    }
    if (spec->version >= 7) {
        int remainder = spec->version << 12;
        for (int bit = 17; bit >= 12; --bit) {
            if ((remainder >> bit) & 1) {
                remainder ^= 0x1F25 << (bit - 12);
            }
        }
        int version_bits = (spec->version << 12) | remainder;
        for (int bit = 0; bit < 18; ++bit) {
            int x = size - 11 + bit % 3;
            int y = bit / 3;
            uint8_t value = (version_bits >> bit) & 1;
            m.set(x, y, value);
            m.set(y, x, value);
            isFunc[static_cast<size_t>(y * size + x)] = 1;
            isFunc[static_cast<size_t>(x * size + y)] = 1;
        }
    }

    m.set(8, size - 8, 1);
    isFunc[static_cast<size_t>((size - 8) * size + 8)] = 1;

    for (int i = 0; i < 9; ++i) {
        if (i != 6) {
            isFunc[static_cast<size_t>(8 * size + i)] = 1;
            isFunc[static_cast<size_t>(i * size + 8)] = 1;
        }
    }
    for (int i = 0; i < 8; ++i) {
        isFunc[static_cast<size_t>((size - 1 - i) * size + 8)] = 1;
        isFunc[static_cast<size_t>(8 * size + (size - 1 - i))] = 1;
    }

    int bitIdx = 0;
    int dir = -1;
    for (int col = size - 1; col >= 1; col -= 2) {
        if (col == 6) {
            col--;
        }
        for (int row = (dir == -1 ? size - 1 : 0);
             row >= 0 && row < size; row += dir) {
            for (int c = 0; c < 2; ++c) {
                int x = col - c;
                int y = row;
                if (isFunc[static_cast<size_t>(y * size + x)]) {
                    continue;
                }
                int byteIdx = bitIdx / 8;
                int bitPos = 7 - (bitIdx % 8);
                uint8_t bit = 0;
                if (byteIdx < static_cast<int>(all.size())) {
                    bit = (all[static_cast<size_t>(byteIdx)] >> bitPos) & 1;
                }
                m.set(x, y, bit);
                ++bitIdx;
            }
        }
        dir = -dir;
    }

    out.m = std::move(m);
    out.isFunc = std::move(isFunc);
    out.size = size;
    return true;
}

} // namespace qr_internal
