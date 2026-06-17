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
};

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

inline bool build_qr_base(const std::string &input, QrBase &out, std::string &err) {
    static const QrSpec specs[] = {
        {1, 21, 19, 7},
        {2, 25, 34, 10},
        {3, 29, 55, 15},
        {4, 33, 80, 20},
    };

    const QrSpec *spec = nullptr;
    int data_len = static_cast<int>(input.size());
    for (const auto &s : specs) {
        int capacityBits = s.dataCodewords * 8;
        int needed = 4 + 8 + data_len * 8;
        if (needed <= capacityBits) {
            spec = &s;
            break;
        }
    }
    if (!spec) {
        err = "QR L supports up to 80 bytes (version 1-4)";
        return false;
    }

    std::vector<int> bits;
    auto append_bits_n = [&](int val, int n) {
        for (int i = n - 1; i >= 0; --i) {
            bits.push_back((val >> i) & 1);
        }
    };

    append_bits_n(0x4, 4); // byte mode
    append_bits_n(data_len, 8);
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
    std::vector<uint8_t> ecc = rs_remainder(data, gen);

    std::vector<uint8_t> all = data;
    all.insert(all.end(), ecc.begin(), ecc.end());

    int size = spec->size;
    ModuleMatrix m(size, size, 0);
    std::vector<uint8_t> isFunc(static_cast<size_t>(size * size), 0);

    draw_finder(m, isFunc, 0, 0);
    draw_finder(m, isFunc, size - 7, 0);
    draw_finder(m, isFunc, 0, size - 7);

    if (spec->version >= 2) {
        int pos[2] = {6, size - 7};
        for (int y : pos) {
            for (int x : pos) {
                if (isFunc[static_cast<size_t>(y * size + x)]) {
                    continue;
                }
                draw_alignment(m, isFunc, x, y);
            }
        }
    }

    for (int i = 8; i < size - 8; ++i) {
        m.set(i, 6, (i % 2 == 0) ? 1 : 0);
        m.set(6, i, (i % 2 == 0) ? 1 : 0);
        isFunc[static_cast<size_t>(6 * size + i)] = 1;
        isFunc[static_cast<size_t>(i * size + 6)] = 1;
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
