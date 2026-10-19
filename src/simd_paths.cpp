#include "simd_paths.h"

#ifdef USE_SSE

#include "encoder_qr_common.h"

#include <emmintrin.h>

namespace {

bool apply_mask_sse(ModuleMatrix &m, const std::vector<uint8_t> &isFunc, std::string &err) {
    if (m.w <= 0 || m.h <= 0) {
        return true;
    }
    if (m.m.size() != isFunc.size()) {
        err = "SSE mask failed: matrix size mismatch";
        return false;
    }

    const __m128i zero = _mm_setzero_si128();

    for (int y = 0; y < m.h; ++y) {
        int x = 0;
        for (; x + 15 < m.w; x += 16) {
            alignas(16) uint8_t mask_bytes[16];
            for (int i = 0; i < 16; ++i) {
                mask_bytes[i] = (((x + i + y) & 1) == 0) ? 1 : 0;
            }
            __m128i vmask = _mm_load_si128(reinterpret_cast<const __m128i *>(mask_bytes));
            __m128i vmat = _mm_loadu_si128(reinterpret_cast<const __m128i *>(
                &m.m[static_cast<size_t>(y * m.w + x)]));
            __m128i vfunc = _mm_loadu_si128(reinterpret_cast<const __m128i *>(
                &isFunc[static_cast<size_t>(y * m.w + x)]));
            __m128i vfunc_zero = _mm_cmpeq_epi8(vfunc, zero);
            __m128i vapply = _mm_and_si128(vmask, vfunc_zero);
            vmat = _mm_xor_si128(vmat, vapply);
            _mm_storeu_si128(reinterpret_cast<__m128i *>(
                                 &m.m[static_cast<size_t>(y * m.w + x)]),
                             vmat);
        }
        for (; x < m.w; ++x) {
            size_t idx = static_cast<size_t>(y * m.w + x);
            if (isFunc[idx]) {
                continue;
            }
            if (((x + y) & 1) == 0) {
                m.m[idx] ^= 1;
            }
        }
    }

    return true;
}

void fill_bytes_sse(uint8_t *dst, size_t count, uint8_t value) {
    __m128i v = _mm_set1_epi8(static_cast<char>(value));
    size_t i = 0;
    for (; i + 15 < count; i += 16) {
        _mm_storeu_si128(reinterpret_cast<__m128i *>(dst + i), v);
    }
    for (; i < count; ++i) {
        dst[i] = value;
    }
}

} // namespace

bool sse_available(std::string &err) {
    (void)err;
    return true;
}

ModuleMatrix linear_to_matrix_sse(const std::vector<uint8_t> &modules, int height, int quiet,
                                  std::string &err) {
    if (height <= 0) {
        err = "SSE linear_to_matrix failed: invalid height";
        return {};
    }
    int w = static_cast<int>(modules.size()) + quiet * 2;
    if (w <= 0) {
        err = "SSE linear_to_matrix failed: invalid width";
        return {};
    }

    ModuleMatrix mat(w, height, 0);
    if (modules.empty()) {
        return mat;
    }

    for (int y = 0; y < height; ++y) {
        uint8_t *row = &mat.m[static_cast<size_t>(y * w + quiet)];
        size_t n = modules.size();
        size_t i = 0;
        for (; i + 15 < n; i += 16) {
            __m128i v = _mm_loadu_si128(reinterpret_cast<const __m128i *>(modules.data() + i));
            _mm_storeu_si128(reinterpret_cast<__m128i *>(row + i), v);
        }
        for (; i < n; ++i) {
            row[i] = modules[i];
        }
    }

    return mat;
}

ModuleMatrix encode_qr_sse(const std::string &input, std::string &err, int version) {
    qr_internal::QrBase base;
    if (!qr_internal::build_qr_base(input, base, err, version)) {
        return {};
    }
    if (!apply_mask_sse(base.m, base.isFunc, err)) {
        return {};
    }
    qr_internal::apply_format_bits(base.m, base.isFunc, base.size);
    return base.m;
}

Bitmap render_bitmap_sse(const ModuleMatrix &mat, int scale, int margin, std::string &err) {
    if (scale <= 0) {
        err = "SSE render_bitmap failed: invalid scale";
        return {};
    }

    int w = (mat.w + margin * 2) * scale;
    int h = (mat.h + margin * 2) * scale;
    if (w <= 0 || h <= 0) {
        err = "SSE render_bitmap failed: invalid dimensions";
        return {};
    }

    Bitmap bmp;
    bmp.w = w;
    bmp.h = h;
    bmp.pixels.resize(static_cast<size_t>(w * h));

    fill_bytes_sse(bmp.pixels.data(), bmp.pixels.size(), 255);

    for (int y = 0; y < mat.h; ++y) {
        for (int x = 0; x < mat.w; ++x) {
            if (!mat.get(x, y)) {
                continue;
            }
            int px = (x + margin) * scale;
            int py = (y + margin) * scale;
            for (int dy = 0; dy < scale; ++dy) {
                uint8_t *row = &bmp.pixels[static_cast<size_t>((py + dy) * w + px)];
                for (int dx = 0; dx < scale; ++dx) {
                    row[dx] = 0;
                }
            }
        }
    }

    return bmp;
}

#endif
