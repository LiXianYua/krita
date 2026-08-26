/*
 *  SPDX-FileCopyrightText: 2023 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISCOSWRITER_H
#define KISCOSWRITER_H

#include <PkVariant.h>
#include "kritapsdutils_export.h"

class KRITAPSDUTILS_EXPORT KisCosWriter
{
public:
    static PkByteArray writeCosFromVariantHash(const PkVariantHash doc);

    static PkByteArray writeTxt2FromVariantHash(const PkVariantHash doc);
};

#endif // KISCOSWRITER_H
