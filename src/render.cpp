#include "render.h"

#include <algorithm>
#include <cerrno>
#include <cstdlib>
#include <iostream>

#include <sys/ioctl.h>
#include <unistd.h>

static int terminal_columns() {
    winsize size{};
    if (isatty(STDOUT_FILENO) && ioctl(STDOUT_FILENO, TIOCGWINSZ, &size) == 0
        && size.ws_col > 0) {
        return size.ws_col;
    }
    const char *env = std::getenv("COLUMNS");
    if (!env || !*env) {
        return 0;
    }
    char *end = nullptr;
    errno = 0;
    long val = std::strtol(env, &end, 10);
    if (errno || end == env || *end != '\0' || val <= 0 || val > 10000) {
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

bool render_ascii(const ModuleMatrix &mat, int hscale, bool half_height,
                  const std::string &terminal_mode, std::string &err, int terminal_scale) {
    if (terminal_scale < 1 || terminal_scale > 8) {
        err = "Terminal scale must be 1-8";
        return false;
    }
    if (hscale != 1) {
        err = "QR terminal output requires --hscale 1; merging modules destroys QR data";
        return false;
    }
    if (terminal_mode != "auto" && terminal_mode != "unicode" && terminal_mode != "ansi") {
        err = "Unknown terminal mode: " + terminal_mode + " (use auto, unicode or ansi)";
        return false;
    }
    const bool tty = isatty(STDOUT_FILENO);
    const char *term = std::getenv("TERM");
    const bool dumb = term && std::string(term) == "dumb";
    const char *program = std::getenv("TERM_PROGRAM");
    const bool vscode = program && std::string(program) == "vscode";
    const bool wsl = std::getenv("WSL_DISTRO_NAME") || std::getenv("WSL_INTEROP");
    // WSL console hosts can render block glyphs differently; colored spaces avoid glyphs.
    const bool ansi = terminal_mode == "ansi"
        || (terminal_mode == "auto" && tty && !dumb && wsl && !vscode);
    if (ansi && tty && dumb) {
        err = "ANSI mode requires a terminal with color support; use PNG output";
        return false;
    }
    const bool color = ansi || (tty && !dumb);
    const bool half = half_height && !ansi;
    const int margin = 4;
    const int width = (mat.w + 2 * margin) * terminal_scale;
    const int height = (mat.h + 2 * margin) * terminal_scale;
    const int columns = width * (half ? 1 : 2);
    const int available = terminal_columns();
    // Reserve one column to avoid right-margin autowrap on console hosts.
    if (available > 0 && columns >= available) {
        err = "Terminal too narrow: need at least " + std::to_string(columns + 1)
            + " columns, have " + std::to_string(available)
            + ". Widen the window, use --terminal unicode, or output PNG";
        return false;
    }
    auto dark = [&](int x, int y) {
        x = x / terminal_scale - margin;
        y = y / terminal_scale - margin;
        return x >= 0 && x < mat.w && y >= 0 && y < mat.h && mat.get(x, y);
    };
    for (int y = 0; y < height; y += half ? 2 : 1) {
        if (color && !ansi) {
            std::cout << "\033[0;38;2;0;0;0;48;2;255;255;255m";
        }
        for (int x = 0; x < width; ++x) {
            const bool top = dark(x, y);
            if (ansi) {
                std::cout << (top ? "\033[0;48;2;0;0;0m" : "\033[0;48;2;255;255;255m")
                          << "  ";
            } else if (half) {
                const bool bottom = dark(x, y + 1);
                std::cout << (top ? (bottom ? "█" : "▀") : (bottom ? "▄" : " "));
            } else {
                std::cout << (top ? "██" : "  ");
            }
        }
        if (color) {
            std::cout << "\033[0m";
        }
        std::cout << "\n";
    }
    return true;
}
