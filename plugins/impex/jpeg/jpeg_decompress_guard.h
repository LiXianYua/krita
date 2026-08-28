#ifndef JPEG_DECOMPRESS_GUARD_H
#define JPEG_DECOMPRESS_GUARD_H

#include <cstdio>
#include <jpeglib.h>

class KisJPEGDecompressGuard
{
public:
    explicit KisJPEGDecompressGuard(j_decompress_ptr decompressor)
        : m_decompressor(decompressor)
    {
    }

    ~KisJPEGDecompressGuard()
    {
        if (m_decompressor) jpeg_destroy_decompress(m_decompressor);
    }

    KisJPEGDecompressGuard(const KisJPEGDecompressGuard &) = delete;
    KisJPEGDecompressGuard &operator=(const KisJPEGDecompressGuard &) = delete;

private:
    j_decompress_ptr m_decompressor;
};

#endif
