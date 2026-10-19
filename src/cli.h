#pragma once

#include <string>

struct Options {
    std::string text;
    std::string sym;
    std::string outPath;
    std::string format;
    std::string terminal_mode = "auto";
    int scale = 4;
    int qr_version = 0;
    int terminal_scale = 1;
    int height = 5;
    int quiet = 10;
    int hscale = 1;
    bool half_height = true;
    bool height_set = false;
    bool use_cuda = false;
    bool use_sse = false;
    bool show_help = false;
};

void print_usage();
bool parse_args(int argc, char **argv, Options &opt, std::string &err);
