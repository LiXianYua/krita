/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SVGTRANSFORMPARSER_H
#define SVGTRANSFORMPARSER_H

#include <PkXmlCompat.h>

#include "kritaflake_export.h"


class KRITAFLAKE_EXPORT SvgTransformParser
{
public:
    SvgTransformParser(const PkString &str);
    bool isValid() const;
    PkTransform transform() const;

private:
    bool m_isValid;
    PkTransform m_transform;
};

#endif // SVGTRANSFORMPARSER_H
