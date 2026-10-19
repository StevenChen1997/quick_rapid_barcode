#include "cli.h"

#include <algorithm>
#include <cstdlib>
#include <charconv>
#include <system_error>
#include <iostream>

void print_usage() {
    std::cout
        << "qrb - Simple barcode generator\n"
        << "Usage:\n"
        << "  ./qrb -t <text> -s <symbology> [options]\n\n"
        << "Symbology:\n"
        << "  ean13 | upc | code128 | code39 | itf | qr\n\n"
        << "Options:\n"
        << "  -t <text>          Input text\n"
        << "  -s <symbology>     Barcode type\n"
        << "  -o <output>        Output file (png/jpg)\n"
        << "  -f <format>        ascii | png | jpg (default: ascii if no -o)\n"
        << "  --qr-version <n>   QR version 1-40; 0 selects smallest (default: 0)\n"
        << "  --terminal-scale <n>  Terminal module magnification 1-8 (default: 1)\n"
        << "  --scale <n>        Scale for pixels (default: 4)\n"
        << "  --height <n>       1D barcode height in modules (default: 5)\n"
        << "  --quiet <n>        Quiet zone in modules (default: 10)\n"
        << "  --hscale <n>       QR terminal scale (must be 1; no lossy compression)\n"
        << "  --terminal <mode>  auto | unicode | ansi (default: auto)\n"
        << "  --half             Unicode half-height output (ANSI uses full rows)\n"
        << "  --cuda             Use CUDA for encode/render (requires USE_CUDA=1 build)\n"
        << "  --sse              Use SSE for encode/render (requires USE_SSE=1 build)\n"
        << "  -h                 Show help\n\n"
        << "Notes:\n"
        << "  QR encoder supports version 1-40 L (byte mode, <= 2953 bytes).\n";
}

bool parse_args(int argc, char **argv, Options &opt, std::string &err) {
    for (int i = 1; i < argc; ++i) {
        std::string arg = argv[i];
        if (arg == "-t" && i + 1 < argc) {
            opt.text = argv[++i];
        } else if (arg == "-s" && i + 1 < argc) {
            opt.sym = argv[++i];
        } else if (arg == "-o" && i + 1 < argc) {
            opt.outPath = argv[++i];
        } else if (arg == "-f" && i + 1 < argc) {
            opt.format = argv[++i];
        } else if ((arg == "--qr-version" || arg == "--terminal-scale") && i + 1 < argc) {
            std::string value = argv[++i];
            int parsed = 0;
            auto result = std::from_chars(value.data(), value.data() + value.size(), parsed);
            int minimum = arg == "--qr-version" ? 0 : 1;
            int maximum = arg == "--qr-version" ? 40 : 8;
            if (result.ec != std::errc() || result.ptr != value.data() + value.size()
                || parsed < minimum || parsed > maximum) {
                err = arg + " must be an integer from " + std::to_string(minimum)
                    + " to " + std::to_string(maximum);
                return false;
            }
            if (arg == "--qr-version") {
                opt.qr_version = parsed;
            } else {
                opt.terminal_scale = parsed;
            }
        } else if (arg == "--scale" && i + 1 < argc) {
            opt.scale = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--height" && i + 1 < argc) {
            opt.height = std::max(1, std::atoi(argv[++i]));
            opt.height_set = true;
        } else if (arg == "--quiet" && i + 1 < argc) {
            opt.quiet = std::max(0, std::atoi(argv[++i]));
        } else if (arg == "--hscale" && i + 1 < argc) {
            opt.hscale = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--terminal" && i + 1 < argc) {
            opt.terminal_mode = argv[++i];
            if (opt.terminal_mode != "auto" && opt.terminal_mode != "unicode"
                && opt.terminal_mode != "ansi") {
                err = "Invalid --terminal mode (use auto, unicode or ansi)";
                return false;
            }
        } else if (arg == "--half") {
            opt.half_height = true;
        } else if (arg == "--cuda") {
            opt.use_cuda = true;
        } else if (arg == "--sse") {
            opt.use_sse = true;
        } else if (arg == "-h" || arg == "--help") {
            opt.show_help = true;
            return true;
        } else {
            err = "Unknown argument: " + arg;
            return false;
        }
    }

    return true;
}
