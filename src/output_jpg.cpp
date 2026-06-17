#include "output_jpg.h"

#include <cstdio>

#ifdef USE_LIBJPEG
#include <jpeglib.h>
#endif

bool write_jpg(const std::string &path, const Bitmap &bmp, std::string &err) {
#ifdef USE_LIBJPEG
    FILE *file = std::fopen(path.c_str(), "wb");
    if (!file) {
        err = "Failed to open output file";
        return false;
    }
    jpeg_compress_struct cinfo;
    jpeg_error_mgr jerr;
    cinfo.err = jpeg_std_error(&jerr);
    jpeg_create_compress(&cinfo);
    jpeg_stdio_dest(&cinfo, file);
    cinfo.image_width = bmp.w;
    cinfo.image_height = bmp.h;
    cinfo.input_components = 1;
    cinfo.in_color_space = JCS_GRAYSCALE;
    jpeg_set_defaults(&cinfo);
    jpeg_set_quality(&cinfo, 90, TRUE);
    jpeg_start_compress(&cinfo, TRUE);
    while (cinfo.next_scanline < cinfo.image_height) {
        JSAMPROW row = const_cast<JSAMPROW>(
            &bmp.pixels[static_cast<size_t>(cinfo.next_scanline * bmp.w)]);
        jpeg_write_scanlines(&cinfo, &row, 1);
    }
    jpeg_finish_compress(&cinfo);
    jpeg_destroy_compress(&cinfo);
    std::fclose(file);
    return true;
#else
    (void)path;
    (void)bmp;
    err = "JPEG output requires libjpeg (build with USE_LIBJPEG=1)";
    return false;
#endif
}
