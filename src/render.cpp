#include "render.h"

#include <algorithm>
#include <cstdlib>
#include <iostream>

static int terminal_columns() {
    const char *env = std::getenv("COLUMNS");
    if (!env || !*env) {
        return 0;
    }
    char *end = nullptr;
    long val = std::strtol(env, &end, 10);
    if (end == env || val <= 0 || val > 10000) {
        return 0;
    }
    return static_cast<int>(val);
}

ModuleMatrix linear_to_matrix(const std::vector<uint8_t> &modules, int height, int quiet) {
    int w = static_cast<int>(modules.size()) + quiet * 2;
    ModuleMatrix mat(w, height, 0);
    for (int y = 0; y < height; ++y) {
        for (size_t i = 0; i < modules.size(); ++i) {
            if (modules[i]) {
                mat.set(static_cast<int>(i) + quiet, y, 1);
            }
        }
    }
    return mat;
}

Bitmap render_bitmap(const ModuleMatrix &mat, int scale, int margin) {
    int w = (mat.w + margin * 2) * scale;
    int h = (mat.h + margin * 2) * scale;
    Bitmap bmp;
    bmp.w = w;
    bmp.h = h;
    bmp.pixels.assign(static_cast<size_t>(w * h), 255);

    for (int y = 0; y < mat.h; ++y) {
        for (int x = 0; x < mat.w; ++x) {
            if (!mat.get(x, y)) {
                continue;
            }
            int px = (x + margin) * scale;
            int py = (y + margin) * scale;
            for (int dy = 0; dy < scale; ++dy) {
                for (int dx = 0; dx < scale; ++dx) {
                    bmp.pixels[static_cast<size_t>((py + dy) * w + (px + dx))] = 0;
                }
            }
        }
    }
    return bmp;
}

void render_ascii(const ModuleMatrix &mat, int hscale, bool half_height) {
    int max_cols = terminal_columns();
    int scaled_w = (mat.w + hscale - 1) / hscale;
    int width = scaled_w;
    if (max_cols > 0 && max_cols < width) {
        width = max_cols;
    }

    if (half_height) {
        for (int y = 0; y < mat.h; y += 2) {
            for (int sx = 0; sx < width; ++sx) {
                int x0 = sx * hscale;
                bool top = false;
                bool bot = false;
                int x1 = std::min(mat.w, x0 + hscale);
                for (int x = x0; x < x1; ++x) {
                    if (mat.get(x, y)) {
                        top = true;
                    }
                    if (y + 1 < mat.h && mat.get(x, y + 1)) {
                        bot = true;
                    }
                    if (top && bot) {
                        break;
                    }
                }
                if (top && bot) {
                    std::cout << "█";
                } else if (top) {
                    std::cout << "▀";
                } else if (bot) {
                    std::cout << "▄";
                } else {
                    std::cout << " ";
                }
            }
            std::cout << "\n";
        }
        return;
    }

    for (int y = 0; y < mat.h; ++y) {
        for (int sx = 0; sx < width; ++sx) {
            int x0 = sx * hscale;
            bool on = false;
            int x1 = std::min(mat.w, x0 + hscale);
            for (int x = x0; x < x1; ++x) {
                if (mat.get(x, y)) {
                    on = true;
                    break;
                }
            }
            std::cout << (on ? "█" : " ");
        }
        std::cout << "\n";
    }
}
