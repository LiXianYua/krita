/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
   SPDX-FileCopyrightText: 2007, 2009 Thomas Zander <zander@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef KOPARAMETERSHAPE_P_H
#define KOPARAMETERSHAPE_P_H

#include "kritaflake_export.h"
#include <KoParameterShape.h>

#include <PkList.h>
#include <PkPoint.h>

class KoParameterShape;

class KRITAFLAKE_EXPORT KoParameterShape::Private
{
public:
    explicit Private();
    explicit Private(const KoParameterShape::Private &rhs);
    virtual ~Private() = default;

    bool parametric;

    /// the handles that the user can grab and change
    PkList<PkPointF> handles;
};

#endif
