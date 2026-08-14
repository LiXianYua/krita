/*
 *  SPDX-FileCopyrightText: 2005 Adrian Page <adrian@pagenet.plus.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_TOOL_SHAPE_H_
#define KIS_TOOL_SHAPE_H_


#include <kritacanvas_export.h>
#include <kconfiggroup.h>

#include <kis_painter.h>

#include "kis_tool_paint.h"
#include "KisSelectionToolFactoryBase.h"
#include "KisToolShapeUtils.h"

class KoCanvasBase;
class KoPathShape;

/**
 * Base for tools specialized in drawing shapes
 */
class KRITACANVAS_EXPORT KisToolShape : public KisToolPaint
{

    Q_OBJECT

public:
    KisToolShape(KoCanvasBase * canvas, const QCursor & cursor);
    ~KisToolShape() override;
    int flags() const override;

public Q_SLOTS:
    void activate(const QSet<KoShape*> &shapes) override;

protected:
    QWidget* createOptionWidget() override;

    KisToolShapeUtils::FillStyle fillStyle();
    KisToolShapeUtils::StrokeStyle strokeStyle();
    QTransform fillTransform();

    qreal currentStrokeWidth() const;

    struct KRITACANVAS_EXPORT ShapeAddInfo {
        bool shouldAddShape = false;
        bool shouldAddSelectionShape = false;

        void markAsSelectionShapeIfNeeded(KoShape *shape) const;
    };

    ShapeAddInfo shouldAddShape(KisNodeSP currentNode) const;

    void addShape(KoShape* shape);

    void addPathShape(KoPathShape* pathShape, const KUndo2MagicString& name);

    /**
     * Use these methods in subclassed to notify when the user starts and
     * finishes making a shape, and override to be notified
     */
    virtual void beginShape() {}
    virtual void endShape() {}

    KConfigGroup m_configGroup;
};

#endif // KIS_TOOL_SHAPE_H_
