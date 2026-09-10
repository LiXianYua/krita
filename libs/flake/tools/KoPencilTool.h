/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007, 2009 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef _KOPENCILTOOL_H_
#define _KOPENCILTOOL_H_

#include "KoFlakeTypes.h"
#include "KoShapeStroke.h"
#include "KoToolBase.h"
#include <PkConfigGroup.h>
#include <qcolor.h>

class KoPathShape;
class KoPathPoint;

#include "kritaflake_export.h"

class KRITAFLAKE_EXPORT KoPencilTool : public KoToolBase
{
public:
    explicit KoPencilTool(KoCanvasBase *canvas);
    ~KoPencilTool() override;

    void paint(PkPainter &painter, const KoViewConverter &converter) override;

    void mousePressEvent(KoPointerEvent *event) override ;
    void mouseMoveEvent(KoPointerEvent *event) override;
    void mouseReleaseEvent(KoPointerEvent *event) override;
    void pkKeyPressEvent(PkToolKeyEvent *event) override;

    void activate(const PkSet<KoShape*> &shapes) override;
    void deactivate() override;

    /**
     * Set the non-widget stroke data copied into newly created paths.
     * The pencil keeps its own copy, so callers may safely reuse or destroy
     * their configuration object after this call.
     */
    void setStrokeTemplate(const KoShapeStroke &stroke);
    const KoShapeStroke &strokeTemplate() const;

protected:

    /**
     * Add path shape to document.
     * This method can be overridden and change the behaviour of the tool. In that case the subclass takes ownership of pathShape.
     * It gets only called, if there are two or more points in the path.
     */
    virtual void addPathShape(KoPathShape* path, bool closePath);

    KoShapeStrokeSP createStroke();
    KoPathShape * path();
    void setFittingError(qreal fittingError);
    qreal getFittingError();
    void setStrokeColor(PkColor color);

private:
    void selectMode(int mode);
    void setOptimize(int state);
    void setDelta(double delta);

protected:
    virtual void slotUpdatePencilCursor();

private:

    qreal lineAngle(const PkPointF &p1, const PkPointF &p2);
    void addPoint(const PkPointF & point);
    void finish(bool closePath);

    /// returns the nearest existing path point
    KoPathPoint* endPointAtPosition(const PkPointF &position);

    /// Connects given path with the ones we hit when starting/finishing
    bool connectPaths(KoPathShape *pathShape, KoPathPoint *pointAtStart, KoPathPoint *pointAtEnd);

    enum PencilMode { ModeRaw, ModeCurve, ModeStraight };

    PencilMode m_mode {ModeCurve};
    bool m_optimizeRaw {false};
    bool m_optimizeCurve {false};
    qreal m_combineAngle {15.0};
    qreal m_fittingError {5.0};
    bool m_close {false};
    PkColor m_strokeColor {Pk::black};
    KoShapeStroke m_strokeTemplate;

    PkList<PkPointF> m_points; // the raw points

    KoPathShape * m_shape {0};
    KoPathPoint *m_existingStartPoint {0}; ///< an existing path point we started a new path at
    KoPathPoint *m_existingEndPoint {0};   ///< an existing path point we finished a new path at
    KoPathPoint *m_hoveredPoint {0}; ///< an existing path end point the mouse is hovering on
    PkConfigGroup m_configGroup;
};

#endif // _KOPENCILTOOL_H_
