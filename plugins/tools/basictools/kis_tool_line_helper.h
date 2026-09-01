/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_TOOL_LINE_HELPER_H
#define __KIS_TOOL_LINE_HELPER_H

#include "kis_tool_freehand_helper.h"


class KisToolLineHelper : private KisToolFreehandHelper
{
public:
    KisToolLineHelper(KisPaintingInformationBuilder *infoBuilder,
                      KoCanvasResourceProvider *resourceManager,
                      const KUndo2MagicString &transactionText);

    ~KisToolLineHelper() override;

    void setEnabled(bool value);
    void setUseSensors(bool value);

    void repaintLine(KisImageWSP image,
                     KisNodeSP node,
                     KisStrokesFacade *strokesFacade);

    void start(KoPointerEvent *event, KoCanvasResourceProvider *resourceManager);
    void addPoint(KoPointerEvent *event, const PkPointF &overridePos = PkPointF());
    void addPoint(KisPaintInformation pi, const PkPointF &overridePos = PkPointF());
    void translatePoints(const PkPointF &offset);
    // overwrites the first and last points, and adjusts the rest of the points to fit the line
    void movePointsTo(const PkPointF& startPoint, const PkPointF& endPoint);
    void end();
    void cancel();
    void clearPoints();
    void clearPaint();

    using KisToolFreehandHelper::isRunning;

private:
    void adjustPointsToDDA(PkVector<KisPaintInformation> &points);

private:
    struct Private;
    Private * const m_d;
};

#endif /* __KIS_TOOL_LINE_HELPER_H */
