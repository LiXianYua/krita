/*
 * SPDX-FileCopyrightText: 2023 Rasyuqa A. H. <qampidh@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef RGBE_IMPORT_UTILS_H_
#define RGBE_IMPORT_UTILS_H_

#include <kis_sequential_iterator.h>

class PkDataStream;
class PkStream;

namespace RGBEIMPORT {
    bool LoadHDR(PkDataStream &s, PkStream *device, const int width, const int height, KisSequentialIterator &it);
}

#endif // RGBE_IMPORT_UTILS_H_
