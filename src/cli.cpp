#include "cli.h"

#include <algorithm>
#include <cstdlib>
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
        << "  --scale <n>        Scale for pixels (default: 4)\n"
        << "  --height <n>       1D barcode height in modules (default: 5)\n"
        << "  --quiet <n>        Quiet zone in modules (default: 10)\n"
        << "  --hscale <n>       ASCII horizontal scale (default: 1)\n"
        << "  --half             ASCII half-height output (default for QR)\n"
        << "  --cuda             Use CUDA for encode/render (requires USE_CUDA=1 build)\n"
        << "  --sse              Use SSE for encode/render (requires USE_SSE=1 build)\n"
        << "  -h                 Show help\n\n"
        << "Notes:\n"
        << "  QR encoder supports version 1-4 L (<= 80 bytes).\n";
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
        } else if (arg == "--scale" && i + 1 < argc) {
            opt.scale = std::max(1, std::atoi(argv[++i]));
        } else if (arg == "--height" && i + 1 < argc) {
            opt.height = std::max(1, std::atoi(argv[++i]));
            opt.height_set = true;
        } else if (arg == "--quiet" && i + 1 < argc) {
            opt.quiet = std::max(0, std::atoi(argv[++i]));
        } else if (arg == "--hscale" && i + 1 < argc) {
            opt.hscale = std::max(1, std::atoi(argv[++i]));
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
