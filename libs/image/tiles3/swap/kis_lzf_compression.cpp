/*
 *  SPDX-FileCopyrightText: 2005-2006 Ariya Hidayat <ariya@kde.org>
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <cstdint>

#include "kis_lzf_compression.h"
#include "kis_debug.h"


#define HASH_LOG  12
#define HASH_SIZE (1<< HASH_LOG)
#define HASH_MASK  (HASH_SIZE-1)

#if !defined _MSC_VER
#pragma GCC diagnostic ignored "-Wcast-align"
#endif
#define UPDATE_HASH(v,p) { v = *((std::uint16_t*)p); v ^= *((std::uint16_t*)(p+1))^(v>>(16-HASH_LOG)); }

#define MAX_COPY       32
#define MAX_LEN       264  /* 256 + 8 */
#define MAX_DISTANCE 8192


// Lossless compression using LZF algorithm, this is faster on modern CPU than
// the original implementation in http://liblzf.plan9.de/
int lzff_compress(const void* input, int length, void* output, int /*maxout*/)
{
    const std::uint8_t* ip = (const std::uint8_t*) input;
    const std::uint8_t* ip_limit = ip + length - MAX_COPY - 4;
    std::uint8_t* op = (std::uint8_t*) output;

    const std::uint8_t* htab[HASH_SIZE];
    const std::uint8_t** hslot;
    std::uint32_t hval;

    std::uint8_t* ref;
    std::int32_t copy;
    std::int32_t len;
    std::int32_t distance;
    std::uint8_t* anchor;

    /* initializes hash table */
    for (hslot = htab; hslot < htab + HASH_SIZE; hslot++)
        *hslot = ip;

    /* we start with literal copy */
    copy = 0;
    *op++ = MAX_COPY - 1;

    /* main loop */
    while (ip < ip_limit) {
        /* find potential match */
        UPDATE_HASH(hval, ip);
        hslot = htab + (hval & HASH_MASK);
        ref = (std::uint8_t*) * hslot;

        /* update hash table */
        *hslot = ip;

        /* find itself? then it's no match */
        if (ip == ref)
            goto literal;

        /* is this a match? check the first 2 bytes */
        if (*((std::uint16_t*)ref) != *((std::uint16_t*)ip))
            goto literal;

        /* now check the 3rd byte */
        if (ref[2] != ip[2])
            goto literal;

        /* calculate distance to the match */
        distance = ip - ref;

        /* skip if too far away */
        if (distance >= MAX_DISTANCE)
            goto literal;

        /* here we have 3-byte matches */
        anchor = (std::uint8_t*)ip;
        len = 3;
        ref += 3;
        ip += 3;

        /* now we have to check how long the match is */
        if (ip < ip_limit - MAX_LEN) {
            while (len < MAX_LEN - 8) {
                /* unroll 8 times */
                if (*ref++ != *ip++) break;
                if (*ref++ != *ip++) break;
                if (*ref++ != *ip++) break;
                if (*ref++ != *ip++) break;
                if (*ref++ != *ip++) break;
                if (*ref++ != *ip++) break;
                if (*ref++ != *ip++) break;
                if (*ref++ != *ip++) break;
                len += 8;
            }
            ip--;
        }
        len = ip - anchor;

        /* just before the last non-matching byte */
        ip = anchor + len;

        /* if we have copied something, adjust the copy count */
        if (copy) {
            /* copy is biased, '0' means 1 byte copy */
            anchor = anchor - copy - 1;
            *(op - copy - 1) = copy - 1;
            copy = 0;
        } else
            /* back, to overwrite the copy count */
            op--;

        /* length is biased, '1' means a match of 3 bytes */
        len -= 2;

        /* distance is also biased */
        distance--;

        /* encode the match */
        if (len < 7)
            *op++ = (len << 5) + (distance >> 8);
        else {
            *op++ = (7 << 5) + (distance >> 8);
            *op++ = len - 7;
        }
        *op++ = (distance & 255);

        /* assuming next will be literal copy */
        *op++ = MAX_COPY - 1;

        /* update the hash at match boundary */
        --ip;
        UPDATE_HASH(hval, ip);
        htab[hval & HASH_MASK] = ip;
        ip++;

        continue;

    literal:
        *op++ = *ip++;
        copy++;
        if (copy >= MAX_COPY) {
            copy = 0;
            *op++ = MAX_COPY - 1;
        }
    }

    /* left-over as literal copy */
    ip_limit = (const std::uint8_t*)input + length;
    while (ip < ip_limit) {
        *op++ = *ip++;
        copy++;
        if (copy == MAX_COPY) {
            copy = 0;
            *op++ = MAX_COPY - 1;
        }
    }

    /* if we have copied something, adjust the copy length */
    if (copy)
        *(op - copy - 1) = copy - 1;
    else
        op--;

    return op - (std::uint8_t*)output;
}

int lzff_decompress(const void* input, int length, void* output, int maxout)
{
    const std::uint8_t* ip = (const std::uint8_t*) input;
    const std::uint8_t* ip_limit  = ip + length - 1;
    std::uint8_t* op = (std::uint8_t*) output;
    std::uint8_t* op_limit = op + maxout;
    std::uint8_t* ref;

    while (ip < ip_limit) {
        std::uint32_t ctrl = (*ip) + 1;
        std::uint32_t ofs = ((*ip) & 31) << 8;
        std::uint32_t len = (*ip++) >> 5;

        if (ctrl < 33) {
            /* literal copy */
            if (op + ctrl > op_limit)
                return 0;

            /* crazy unrolling */
            if (ctrl) {
                *op++ = *ip++;
                ctrl--;

                if (ctrl) {
                    *op++ = *ip++;
                    ctrl--;

                    if (ctrl) {
                        *op++ = *ip++;
                        ctrl--;

                        for (; ctrl; ctrl--)
                            *op++ = *ip++;
                    }
                }
            }
        } else {
            /* back reference */
            len--;
            ref = op - ofs;
            ref--;

            if (len == 7 - 1)
                len += *ip++;

            ref -= *ip++;

            if (op + len + 3 > op_limit)
                return 0;

            if (ref < (std::uint8_t *)output)
                return 0;

            *op++ = *ref++;
            *op++ = *ref++;
            *op++ = *ref++;
            if (len)
                for (; len; --len)
                    *op++ = *ref++;
        }
    }

    return op - (std::uint8_t*)output;
}



KisLzfCompression::KisLzfCompression()
{
    /**
     * TODO: make working memory htab[HASH_SIZE] be in heap
     * and shared inside the class
     */

}

KisLzfCompression::~KisLzfCompression()
{
}

std::int32_t KisLzfCompression::compress(const std::uint8_t* input, std::int32_t inputLength, std::uint8_t* output, std::int32_t outputLength)
{
    return lzff_compress(input, inputLength, output, outputLength);
}

std::int32_t KisLzfCompression::decompress(const std::uint8_t* input, std::int32_t inputLength, std::uint8_t* output, std::int32_t outputLength)
{
    return lzff_decompress(input, inputLength, output, outputLength);
}

std::int32_t KisLzfCompression::outputBufferSize(std::int32_t dataSize)
{
    // WARNING: Copy-pasted from LZO samples, do not know how to prove it
    return dataSize + dataSize / 16 + 64 + 3;
}
