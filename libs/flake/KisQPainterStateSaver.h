/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISQPAINTERSTATESAVER_H
#define KISQPAINTERSTATESAVER_H

#include "kritaflake_export.h"

class PkPainter;

class KRITAFLAKE_EXPORT KisQPainterStateSaver
{
public:
    KisQPainterStateSaver(PkPainter *painter);
    ~KisQPainterStateSaver();

private:
    KisQPainterStateSaver(const KisQPainterStateSaver &rhs);
    PkPainter *m_painter;
};

#endif // KISQPAINTERSTATESAVER_H
