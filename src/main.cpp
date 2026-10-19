#include "cli.h"
#include "decoder.h"
#include "encoder_pdf417.h"
#include "file_io.h"
#include "encoder_qr.h"
#include "encoders_1d.h"
#include "output_jpg.h"
#include "output_png.h"
#include "render.h"
#ifdef USE_CUDA
#include "cuda_paths.h"
#endif
#ifdef USE_SSE
#include "simd_paths.h"
#endif

#include <cctype>
#include <iostream>

static std::string to_lower(std::string s) {
    for (char &c : s) {
        c = static_cast<char>(std::tolower(static_cast<unsigned char>(c)));
    }
    return s;
}

int main(int argc, char **argv) {
    Options opt;
    std::string err;

    if (!parse_args(argc, argv, opt, err)) {
        std::cerr << err << "\n";
        return 1;
    }

    if (opt.show_help) {
        print_usage();
        return 0;
    }

    if (!opt.decode_path.empty()) {
        std::string payload;
        if (!decode_image(opt.decode_path, payload, err) || !write_payload(opt.outPath, payload, err)) {
            std::cerr << err << "\n";
            return 1;
        }
        return 0;
    }
    if (!opt.input_file.empty() && !read_file(opt.input_file, opt.text, 1024 * 1024, err)) {
        std::cerr << err << "\n";
        return 1;
    }

    if (opt.text.empty() || opt.sym.empty()) {
        print_usage();
        return 1;
    }

    bool use_cuda = opt.use_cuda;
    bool use_sse = opt.use_sse;
    if (use_cuda && use_sse) {
        std::cerr << "Choose only one acceleration path: --cuda or --sse\n";
        return 1;
    }
#ifndef USE_CUDA
    if (use_cuda) {
        std::cerr << "CUDA requested but not enabled (build with USE_CUDA=1)\n";
        return 1;
    }
#else
    if (use_cuda) {
        std::string cuda_err;
        if (!cuda_available(cuda_err)) {
            std::cerr << cuda_err << "\n";
            return 1;
        }
    }
#endif
#ifndef USE_SSE
    if (use_sse) {
        std::cerr << "SSE requested but not enabled (build with USE_SSE=1)\n";
        return 1;
    }
#else
    if (use_sse) {
        std::string sse_err;
        if (!sse_available(sse_err)) {
            std::cerr << sse_err << "\n";
            return 1;
        }
    }
#endif

    std::string sym = to_lower(opt.sym);
    std::string format = opt.format;

    if (format.empty()) {
        if (!opt.outPath.empty()) {
            auto dot = opt.outPath.find_last_of('.');
            if (dot != std::string::npos) {
                format = opt.outPath.substr(dot + 1);
            }
        }
        if (format.empty()) {
            format = "ascii";
        }
    }
    format = to_lower(format);

    ModuleMatrix mat;
    if (sym == "ean13") {
        auto bits = encode_ean13(opt.text, err);
        if (!err.empty()) {
            std::cerr << err << "\n";
            return 1;
        }
        if (use_cuda) {
#ifdef USE_CUDA
            mat = linear_to_matrix_cuda(bits, opt.height, opt.quiet, err);
#endif
        } else if (use_sse) {
#ifdef USE_SSE
            mat = linear_to_matrix_sse(bits, opt.height, opt.quiet, err);
#endif
        } else {
            mat = linear_to_matrix(bits, opt.height, opt.quiet);
        }
    } else if (sym == "upc") {
        auto bits = encode_upc(opt.text, err);
        if (!err.empty()) {
            std::cerr << err << "\n";
            return 1;
        }
        if (use_cuda) {
#ifdef USE_CUDA
            mat = linear_to_matrix_cuda(bits, opt.height, opt.quiet, err);
#endif
        } else if (use_sse) {
#ifdef USE_SSE
            mat = linear_to_matrix_sse(bits, opt.height, opt.quiet, err);
#endif
        } else {
            mat = linear_to_matrix(bits, opt.height, opt.quiet);
        }
    } else if (sym == "code39") {
        auto bits = encode_code39(opt.text, err);
        if (!err.empty()) {
            std::cerr << err << "\n";
            return 1;
        }
        if (use_cuda) {
#ifdef USE_CUDA
            mat = linear_to_matrix_cuda(bits, opt.height, opt.quiet, err);
#endif
        } else if (use_sse) {
#ifdef USE_SSE
            mat = linear_to_matrix_sse(bits, opt.height, opt.quiet, err);
#endif
        } else {
            mat = linear_to_matrix(bits, opt.height, opt.quiet);
        }
    } else if (sym == "itf") {
        auto bits = encode_itf(opt.text, err);
        if (!err.empty()) {
            std::cerr << err << "\n";
            return 1;
        }
        if (use_cuda) {
#ifdef USE_CUDA
            mat = linear_to_matrix_cuda(bits, opt.height, opt.quiet, err);
#endif
        } else if (use_sse) {
#ifdef USE_SSE
            mat = linear_to_matrix_sse(bits, opt.height, opt.quiet, err);
#endif
        } else {
            mat = linear_to_matrix(bits, opt.height, opt.quiet);
        }
    } else if (sym == "code128") {
        auto bits = encode_code128(opt.text, err);
        if (!err.empty()) {
            std::cerr << err << "\n";
            return 1;
        }
        if (use_cuda) {
#ifdef USE_CUDA
            mat = linear_to_matrix_cuda(bits, opt.height, opt.quiet, err);
#endif
        } else if (use_sse) {
#ifdef USE_SSE
            mat = linear_to_matrix_sse(bits, opt.height, opt.quiet, err);
#endif
        } else {
            mat = linear_to_matrix(bits, opt.height, opt.quiet);
        }
    } else if (sym == "pdf417") {
        mat = encode_pdf417(opt.text, opt.pdf417_columns, opt.pdf417_ecc, err);
    } else if (sym == "qr") {
        if (use_cuda) {
#ifdef USE_CUDA
            mat = encode_qr_cuda(opt.text, err, opt.qr_version);
#endif
        } else if (use_sse) {
#ifdef USE_SSE
            mat = encode_qr_sse(opt.text, err, opt.qr_version);
#endif
        } else {
            mat = encode_qr(opt.text, err, opt.qr_version);
        }
    } else {
        std::cerr << "Unknown symbology: " << sym << "\n";
        return 1;
    }

    if (!err.empty()) {
        std::cerr << err << "\n";
        return 1;
    }

    if (format == "ascii") {
        if (sym != "qr") {
            std::cerr << "ASCII output is supported for QR only; use png/jpg for PDF417 and 1D barcodes\n";
            return 1;
        }
        if (!render_ascii(mat, opt.hscale, opt.half_height, opt.terminal_mode, err, opt.terminal_scale)) {
            std::cerr << err << "\n";
            return 1;
        }
        return 0;
    }

    if (format != "png" && format != "jpg" && format != "jpeg") {
        std::cerr << "Unsupported format: " << format << "\n";
        return 1;
    }

    if (opt.outPath.empty()) {
        std::cerr << "Output file required for image formats\n";
        return 1;
    }

    int margin = (sym == "qr") ? 4 : 2;
    const long long pixel_width = (static_cast<long long>(mat.w) + margin * 2) * opt.scale;
    const long long pixel_height = (static_cast<long long>(mat.h) + margin * 2) * opt.scale;
    if (pixel_width > 16384 || pixel_height > 16384 || pixel_width * pixel_height > 16 * 1024 * 1024) {
        std::cerr << "Output exceeds 16384 pixels per side or 16 megapixels; reduce --scale\n";
        return 1;
    }
    Bitmap bmp;
    if (use_cuda) {
#ifdef USE_CUDA
        bmp = render_bitmap_cuda(mat, opt.scale, margin, err);
#endif
    } else if (use_sse) {
#ifdef USE_SSE
        bmp = render_bitmap_sse(mat, opt.scale, margin, err);
#endif
    } else {
        bmp = render_bitmap(mat, opt.scale, margin);
    }
    if (!err.empty()) {
        std::cerr << err << "\n";
        return 1;
    }

    if (format == "png") {
        if (!write_png(opt.outPath, bmp, err)) {
            std::cerr << err << "\n";
            return 1;
        }
    } else {
        if (!write_jpg(opt.outPath, bmp, err)) {
            std::cerr << err << "\n";
            return 1;
        }
    }

    return 0;
}
