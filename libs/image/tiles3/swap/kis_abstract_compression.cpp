/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>

#include "kis_abstract_compression.h"

KisAbstractCompression::KisAbstractCompression()
{
}

KisAbstractCompression::~KisAbstractCompression()
{
}

void KisAbstractCompression::adjustForDataSize(std::int32_t dataSize)
{
    (void)(dataSize);
}

void KisAbstractCompression::linearizeColors(std::uint8_t *input, std::uint8_t *output,
                                             std::int32_t dataSize, std::int32_t pixelSize)
{
    std::uint8_t *outputByte = output;
    std::uint8_t *lastByte = input + dataSize -1;

    for(std::int32_t i = 0; i < pixelSize; i++) {
        std::uint8_t *inputByte = input + i;
        while (inputByte <= lastByte) {
            *outputByte = *inputByte;
            outputByte++;
            inputByte+=pixelSize;
        }
    }
}

void KisAbstractCompression::delinearizeColors(std::uint8_t *input, std::uint8_t *output,
                                               std::int32_t dataSize, std::int32_t pixelSize)
{
    /**
     * In the beginning, i wrote "delinearization" in a way,
     * that looks like a "linearization", but it turned to be quite
     * inefficient. It seems like reading from random positions is
     * much faster than writing to random areas. So this version is
     * 13% faster.
     */

    std::uint8_t *outputByte = output;
    std::uint8_t *lastByte = output + dataSize -1;

    std::int32_t strideSize = dataSize / pixelSize;
    std::uint8_t *startByte = input;

    while (outputByte <= lastByte) {
        std::uint8_t *inputByte = startByte;

        for(std::int32_t i = 0; i < pixelSize; i++) {
            *outputByte = *inputByte;
            outputByte++;
            inputByte += strideSize;
        }

        startByte++;
    }
}
