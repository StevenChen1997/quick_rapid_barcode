#pragma once

#include <cstdint>
#include <vector>

struct ModuleMatrix {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> m;

    ModuleMatrix() = default;
    ModuleMatrix(int width, int height, uint8_t value = 0)
        : w(width), h(height), m(static_cast<size_t>(w * h), value) {}

    uint8_t get(int x, int y) const {
        return m[static_cast<size_t>(y * w + x)];
    }

    void set(int x, int y, uint8_t v) {
        m[static_cast<size_t>(y * w + x)] = v;
    }
};

struct Bitmap {
    int w = 0;
    int h = 0;
    std::vector<uint8_t> pixels; // 0 = black, 255 = white
};
