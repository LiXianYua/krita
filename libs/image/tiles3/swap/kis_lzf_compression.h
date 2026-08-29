/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_LZF_COMPRESSION_H
#define __KIS_LZF_COMPRESSION_H

#include <cstdint>

#include "kis_abstract_compression.h"

class KRITAIMAGE_EXPORT KisLzfCompression : public KisAbstractCompression
{
public:
    KisLzfCompression();
    ~KisLzfCompression() override;

    std::int32_t compress(const std::uint8_t* input, std::int32_t inputLength, std::uint8_t* output, std::int32_t outputLength) override;
    std::int32_t decompress(const std::uint8_t* input, std::int32_t inputLength, std::uint8_t* output, std::int32_t outputLength) override;

    std::int32_t outputBufferSize(std::int32_t dataSize) override;

    //void adjustForDataSize(std::int32_t dataSize);
};

#endif /* __KIS_LZF_COMPRESSION_H */
