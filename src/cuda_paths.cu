#include "cuda_paths.h"

#include "encoder_qr_common.h"

#include <cuda_runtime.h>

#include <string>
#include <vector>

namespace {

bool cuda_check(cudaError_t status, std::string &err, const char *where) {
    if (status == cudaSuccess) {
        return true;
    }
    err = std::string(where) + ": " + cudaGetErrorString(status);
    return false;
}

bool ensure_cuda_device(std::string &err) {
    int count = 0;
    cudaError_t status = cudaGetDeviceCount(&count);
    if (!cuda_check(status, err, "cudaGetDeviceCount")) {
        return false;
    }
    if (count <= 0) {
        err = "No CUDA device available";
        return false;
    }
    return true;
}

template <typename T>
struct DeviceBuffer {
    T *ptr = nullptr;
    size_t count = 0;

    bool alloc(size_t n, std::string &err, const char *where) {
        count = n;
        if (count == 0) {
            return true;
        }
        return cuda_check(cudaMalloc(&ptr, count * sizeof(T)), err, where);
    }

    ~DeviceBuffer() {
        if (ptr) {
            cudaFree(ptr);
        }
    }
};

__global__ void mask_kernel(uint8_t *m, const uint8_t *isFunc, int w, int h) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = w * h;
    if (idx >= total) {
        return;
    }
    if (isFunc[idx]) {
        return;
    }
    int x = idx % w;
    int y = idx / w;
    if (((x + y) & 1) == 0) {
        m[idx] ^= 1;
    }
}

__global__ void linear_to_matrix_kernel(const uint8_t *modules, int moduleCount, int height,
                                        int quiet, uint8_t *out, int w, int h) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = w * h;
    if (idx >= total) {
        return;
    }
    int x = idx % w;
    int y = idx / w;
    if (y >= height) {
        return;
    }
    if (x >= quiet && x < quiet + moduleCount) {
        uint8_t v = modules[x - quiet];
        out[y * w + x] = v ? 1 : 0;
    } else {
        out[y * w + x] = 0;
    }
}

__global__ void render_bitmap_kernel(const uint8_t *mat, int matW, int matH, uint8_t *pixels,
                                     int outW, int outH, int scale, int margin) {
    int idx = blockIdx.x * blockDim.x + threadIdx.x;
    int total = outW * outH;
    if (idx >= total) {
        return;
    }
    int x = idx % outW;
    int y = idx / outW;
    int mx = x / scale - margin;
    int my = y / scale - margin;
    uint8_t val = 255;
    if (mx >= 0 && my >= 0 && mx < matW && my < matH) {
        if (mat[my * matW + mx]) {
            val = 0;
        }
    }
    pixels[idx] = val;
}

bool apply_mask_cuda(ModuleMatrix &m, const std::vector<uint8_t> &isFunc, std::string &err) {
    if (m.w <= 0 || m.h <= 0) {
        return true;
    }
    if (m.m.size() != isFunc.size()) {
        err = "CUDA mask failed: matrix size mismatch";
        return false;
    }

    DeviceBuffer<uint8_t> d_mat;
    DeviceBuffer<uint8_t> d_isFunc;
    size_t total = static_cast<size_t>(m.w * m.h);
    if (!d_mat.alloc(total, err, "cudaMalloc(m)") ||
        !d_isFunc.alloc(total, err, "cudaMalloc(isFunc)")) {
        return false;
    }

    if (!cuda_check(cudaMemcpy(d_mat.ptr, m.m.data(), total, cudaMemcpyHostToDevice), err,
                    "cudaMemcpy(m)") ||
        !cuda_check(cudaMemcpy(d_isFunc.ptr, isFunc.data(), total, cudaMemcpyHostToDevice), err,
                    "cudaMemcpy(isFunc)")) {
        return false;
    }

    int threads = 256;
    int blocks = static_cast<int>((total + threads - 1) / threads);
    mask_kernel<<<blocks, threads>>>(d_mat.ptr, d_isFunc.ptr, m.w, m.h);
    if (!cuda_check(cudaGetLastError(), err, "mask_kernel launch") ||
        !cuda_check(cudaDeviceSynchronize(), err, "mask_kernel sync")) {
        return false;
    }

    if (!cuda_check(cudaMemcpy(m.m.data(), d_mat.ptr, total, cudaMemcpyDeviceToHost), err,
                    "cudaMemcpy(m back)")) {
        return false;
    }

    return true;
}

} // namespace

bool cuda_available(std::string &err) {
    return ensure_cuda_device(err);
}

ModuleMatrix linear_to_matrix_cuda(const std::vector<uint8_t> &modules, int height, int quiet,
                                   std::string &err) {
    if (!ensure_cuda_device(err)) {
        return {};
    }

    int moduleCount = static_cast<int>(modules.size());
    int w = moduleCount + quiet * 2;
    int h = height;
    if (w <= 0 || h <= 0) {
        err = "CUDA linear_to_matrix failed: invalid dimensions";
        return {};
    }

    DeviceBuffer<uint8_t> d_modules;
    DeviceBuffer<uint8_t> d_out;
    size_t moduleBytes = static_cast<size_t>(moduleCount);
    size_t total = static_cast<size_t>(w * h);
    if (!d_modules.alloc(moduleBytes, err, "cudaMalloc(modules)") ||
        !d_out.alloc(total, err, "cudaMalloc(out)")) {
        return {};
    }

    if (moduleBytes > 0 &&
        !cuda_check(cudaMemcpy(d_modules.ptr, modules.data(), moduleBytes, cudaMemcpyHostToDevice),
                    err, "cudaMemcpy(modules)")) {
        return {};
    }

    int threads = 256;
    int blocks = static_cast<int>((total + threads - 1) / threads);
    linear_to_matrix_kernel<<<blocks, threads>>>(d_modules.ptr, moduleCount, h, quiet, d_out.ptr, w,
                                                 h);
    if (!cuda_check(cudaGetLastError(), err, "linear_to_matrix_kernel launch") ||
        !cuda_check(cudaDeviceSynchronize(), err, "linear_to_matrix_kernel sync")) {
        return {};
    }

    ModuleMatrix mat(w, h, 0);
    if (!cuda_check(cudaMemcpy(mat.m.data(), d_out.ptr, total, cudaMemcpyDeviceToHost), err,
                    "cudaMemcpy(out back)")) {
        return {};
    }
    return mat;
}

ModuleMatrix encode_qr_cuda(const std::string &input, std::string &err, int version) {
    if (!ensure_cuda_device(err)) {
        return {};
    }

    qr_internal::QrBase base;
    if (!qr_internal::build_qr_base(input, base, err, version)) {
        return {};
    }

    if (!apply_mask_cuda(base.m, base.isFunc, err)) {
        return {};
    }
    qr_internal::apply_format_bits(base.m, base.isFunc, base.size);
    return base.m;
}

Bitmap render_bitmap_cuda(const ModuleMatrix &mat, int scale, int margin, std::string &err) {
    if (!ensure_cuda_device(err)) {
        return {};
    }

    int w = (mat.w + margin * 2) * scale;
    int h = (mat.h + margin * 2) * scale;
    if (w <= 0 || h <= 0) {
        err = "CUDA render_bitmap failed: invalid dimensions";
        return {};
    }

    DeviceBuffer<uint8_t> d_mat;
    DeviceBuffer<uint8_t> d_pixels;
    size_t matBytes = static_cast<size_t>(mat.w * mat.h);
    size_t pixelBytes = static_cast<size_t>(w * h);

    if (!d_mat.alloc(matBytes, err, "cudaMalloc(mat)") ||
        !d_pixels.alloc(pixelBytes, err, "cudaMalloc(pixels)")) {
        return {};
    }

    if (matBytes > 0 &&
        !cuda_check(cudaMemcpy(d_mat.ptr, mat.m.data(), matBytes, cudaMemcpyHostToDevice), err,
                    "cudaMemcpy(mat)")) {
        return {};
    }

    int threads = 256;
    int blocks = static_cast<int>((pixelBytes + threads - 1) / threads);
    render_bitmap_kernel<<<blocks, threads>>>(d_mat.ptr, mat.w, mat.h, d_pixels.ptr, w, h, scale,
                                              margin);
    if (!cuda_check(cudaGetLastError(), err, "render_bitmap_kernel launch") ||
        !cuda_check(cudaDeviceSynchronize(), err, "render_bitmap_kernel sync")) {
        return {};
    }

    Bitmap bmp;
    bmp.w = w;
    bmp.h = h;
    bmp.pixels.assign(pixelBytes, 255);
    if (!cuda_check(cudaMemcpy(bmp.pixels.data(), d_pixels.ptr, pixelBytes,
                               cudaMemcpyDeviceToHost),
                    err, "cudaMemcpy(pixels back)")) {
        return {};
    }

    return bmp;
}
