/*
 * SPDX-FileCopyrightText: 2009 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "compression_test.h"

#include <PkAuxTypes.h>
#include <PkDataStream.h>

#include <cstdlib>
#include <cstring>

#include <kis_debug.h>

#include <compression.h>

// 字节数组(const char*) 的 Pk 对应：PkByteArray 构造需要长度，这里按 strlen 包一层。
static PkByteArray pba(const char *s)
{
    return PkByteArray(s, static_cast<int>(std::strlen(s)));
}

void CompressionTest::testCompressionRLE()
{
    PkByteArray ba = pba("Twee eeee aaaaa asdasda47892347981    wwwwwwwwwwwwWWWWWWWWWW");
    PkByteArray compressed = Compression::compress(ba, psd_compression_type::RLE);
    PK_VERIFY(compressed.size() > 0);
    dbgKrita << compressed.size() << "uncompressed" << ba.size();

    PkByteArray uncompressed = Compression::uncompress(ba.size(), compressed, psd_compression_type::RLE);
    PK_VERIFY(uncompressed.size() > 0);
    PK_VERIFY(ba == uncompressed);

    ba.resize(0);
    PkDataStream ds(&ba, PkStream::WriteOnly);
    for (int i = 0; i < 500; ++i) {
        ds << rand();
    }
    compressed = Compression::compress(ba, psd_compression_type::RLE);
    dbgKrita << compressed.size() << "uncompressed" << ba.size();
    PK_VERIFY(compressed.size() > 0);
    uncompressed = Compression::uncompress(ba.size(), compressed, psd_compression_type::RLE);
    PK_VERIFY(uncompressed.size() > 0);
    PK_VERIFY(ba == uncompressed);
}

void CompressionTest::testCompressionZIP()
{
    PkByteArray ba = pba("Twee eeee aaaaa asdasda47892347981    wwwwwwwwwwwwWWWWWWWWWW");
    PkByteArray compressed = Compression::compress(ba, psd_compression_type::ZIP);
    PK_VERIFY(compressed.size() > 0);
    dbgKrita << compressed.size() << "uncompressed" << ba.size();

    PkByteArray uncompressed = Compression::uncompress(ba.size(), compressed, psd_compression_type::ZIP);
    PK_VERIFY(uncompressed.size() > 0);
    PK_VERIFY(ba == uncompressed);

    ba.resize(0);
    PkDataStream ds(&ba, PkStream::WriteOnly);
    for (int i = 0; i < 500; ++i) {
        ds << rand();
    }
    compressed = Compression::compress(ba, psd_compression_type::ZIP);
    PK_VERIFY(compressed.size() > 0);
    dbgKrita << compressed.size() << "uncompressed" << ba.size();
    uncompressed = Compression::uncompress(ba.size(), compressed, psd_compression_type::ZIP);
    PK_VERIFY(uncompressed.size() > 0);
    PK_VERIFY(ba == uncompressed);
}

void CompressionTest::testCompressionUncompressed()
{
    PkByteArray ba = pba("Twee eeee aaaaa asdasda47892347981    wwwwwwwwwwwwWWWWWWWWWW");
    PkByteArray compressed = Compression::compress(ba, psd_compression_type::Uncompressed);
    PK_VERIFY(compressed.size() > 0);
    dbgKrita << compressed.size() << "uncompressed" << ba.size();

    PkByteArray uncompressed = Compression::uncompress(ba.size(), compressed, psd_compression_type::Uncompressed);
    PK_VERIFY(uncompressed.size() > 0);
    PK_VERIFY(ba == uncompressed);

    ba.resize(0);
    PkDataStream ds(&ba, PkStream::WriteOnly);
    for (int i = 0; i < 500; ++i) {
        ds << rand();
    }
    compressed = Compression::compress(ba, psd_compression_type::Uncompressed);
    dbgKrita << compressed.size() << "uncompressed" << ba.size();
    PK_VERIFY(compressed.size() > 0);
    uncompressed = Compression::uncompress(ba.size(), compressed, psd_compression_type::Uncompressed);
    PK_VERIFY(uncompressed.size() > 0);
    PK_VERIFY(ba == uncompressed);
}

#ifdef PK_SHELL_MOC_BINDER
#include "pk_binder_compression_test.inc"
#endif

PK_TEST_GUILESS_MAIN(CompressionTest)
