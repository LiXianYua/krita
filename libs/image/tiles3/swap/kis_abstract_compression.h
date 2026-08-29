/*
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ABSTRACT_COMPRESSION_H
#define __KIS_ABSTRACT_COMPRESSION_H

#include <cstdint>

#include "kritaimage_export.h"

/**
 * Base class for compression operations
 */

class KRITAIMAGE_EXPORT KisAbstractCompression
{
public:
    KisAbstractCompression();
    virtual ~KisAbstractCompression();

    /**
     * Compresses \p input buffer into \p output buffer.
     * WARNING: Be careful, output buffer must be at least
     * outputBufferSize(inputLength) size!
     * \param input the input
     * \param inputLength the input length
     * \param output the output
     * \param outputLength is not used!
     * \return number of bytes written to the output buffer
     * and 0 if error occurred.
     *
     * \see outputBufferSize()
     */
    virtual std::int32_t compress(const std::uint8_t* input, std::int32_t inputLength, std::uint8_t* output, std::int32_t outputLength) = 0;

    /**
     * Decompresses \p input buffer into \p output buffer.
     * WARNING: output buffer must be able to fit the input data
     * \param input the input
     * \param inputLength the input length
     * \param output the output
     * \param outputLength is not used!
     * \return number of bytes written to the output buffer
     * and 0 if error occurred.
     */
    virtual std::int32_t decompress(const std::uint8_t* input, std::int32_t inputLength, std::uint8_t* output, std::int32_t outputLength) = 0;

    /**
     * Returns minimal allowed size of output buffer for compression
     */
    virtual std::int32_t outputBufferSize(std::int32_t dataSize) = 0;

    /**
     * Some algorithms may decide to optimize them work depending on
     * the usual size of the data.
     * Default implementation of KisAbstractCompression class does nothing.
     */
    virtual void adjustForDataSize(std::int32_t dataSize);

public:
    /**
     * Additional interface for jumbling color channels order
     */

    /**
     * e.g. RGBARGBARGBA -> RRRGGGBBBAAA
     * NOTE: performs mixing of bytes, not channels!
     */
    static void linearizeColors(std::uint8_t *input, std::uint8_t *output,
                                std::int32_t dataSize, std::int32_t pixelSize);
    /**
     * e.g. RRRGGGBBBAAA -> RGBARGBARGBA
     * NOTE: performs mixing of bytes, not channels!
     */
    static void delinearizeColors(std::uint8_t *input, std::uint8_t *output,
                                  std::int32_t dataSize, std::int32_t pixelSize);
};

#endif /* __KIS_ABSTRACT_COMPRESSION_H */
