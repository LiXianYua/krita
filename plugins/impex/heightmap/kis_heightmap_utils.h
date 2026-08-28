/*
 *  SPDX-FileCopyrightText: 2017 Victor Wåhlström <victor.wahlstrom@initiali.se>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#ifndef _KIS_HEIGHTMAP_UTILS_H_
#define _KIS_HEIGHTMAP_UTILS_H_

#include <KoID.h>

class PkByteArray;

namespace KisHeightmapUtils
{
KoID mimeTypeToKoID(const PkByteArray& mimeType);
}

#endif // _KIS_HEIGHTMAP_UTILS_H_
