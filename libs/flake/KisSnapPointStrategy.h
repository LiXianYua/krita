/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISSNAPPOINTSTRATEGY_H
#define KISSNAPPOINTSTRATEGY_H

#include <PkScopedPointer.h>

#include "KoSnapStrategy.h"
#include "kritaflake_export.h"

/**
 * The KisSnapPointStrategy class is a custom strategy that allows snapping to
 * arbitrary points on canvas, not linked to any real objects. It can be used,
 * for example, for snapping to the *previous position* of the handle, while it
 * is dragging by the user.
 */

class KRITAFLAKE_EXPORT KisSnapPointStrategy : public KoSnapStrategy
{
public:
    KisSnapPointStrategy(KoSnapGuide::Strategy type = KoSnapGuide::CustomSnapping);
    ~KisSnapPointStrategy() override;

    bool snap(const PkPointF &mousePosition, KoSnapProxy * proxy, qreal maxSnapDistance) override;
    PkPainterPath decoration(const KoViewConverter &converter) const override;

     void addPoint(const PkPointF &pt);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif // KISSNAPPOINTSTRATEGY_H
