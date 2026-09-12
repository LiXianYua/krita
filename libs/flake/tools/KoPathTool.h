/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006-2012 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2006, 2007 Thorsten Zachmann <zachmann@kde.org>
 * SPDX-FileCopyrightText: 2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2007 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOPATHTOOL_H
#define KOPATHTOOL_H

#include "KoPathShape.h"
#include "KoToolBase.h"
#include "KoPathToolSelection.h"
#include "kis_signal_auto_connection.h"
#include <PkList.h>
#include "KoCanvasCursorHost.h"
#include <PkConnection.h>
#include <KoShapeFillResourceConnector.h>
#include "KoPathPointTypeCommand.h"
#include <KoSvgTextShapeOutlineHelper.h>
// 宿主动作载体。本 TU 组在 native 桶里编（见 libs/flake/CMakeLists.txt 的 R-57 注释），
// 这条 include 解析到 flake 自有的 `flake/noqt-compat/QAction`（派生 PkObject、
// 带真 `triggered()` 信号与 enabled/property/快捷键载荷），与已在本桶里这么用的
// `KoToolFactoryBase.cpp` 是同一形态；qt 桶的消费者仍拿到真 Qt 头。
#include <QAction>

class QActionGroup;
class QButtonGroup;
class KoCanvasBase;
class KoInteractionStrategy;
class KoPathToolHandle;
class KoParameterShape;
class KUndo2Command;


/// The tool for editing a KoPathShape or a KoParameterShape.
/// See KoCreatePathTool for code handling the initial path creation.
class KRITAFLAKE_EXPORT KoPathTool : public KoToolBase
{
public:
    explicit KoPathTool(KoCanvasBase *canvas);
    ~KoPathTool() override;

    void paint(PkPainter &painter, const KoViewConverter &converter) override;
    void repaintDecorations() override;
    PkRectF decorationsRect() const override;
    void mousePressEvent(KoPointerEvent *event) override;
    void mouseMoveEvent(KoPointerEvent *event) override;
    void mouseReleaseEvent(KoPointerEvent *event) override;
    void pkKeyPressEvent(PkToolKeyEvent *event) override;
    void pkKeyReleaseEvent(PkToolKeyEvent *event) override;
    void mouseDoubleClickEvent(KoPointerEvent *event) override;
    void activate(const PkSet<KoShape*> &shapes) override;
    void deactivate() override;
    void deleteSelection() override;
    KoToolSelection* selection() override;
    void requestUndoDuringStroke() override;
    void requestStrokeCancellation() override;
    void requestStrokeEnd() override;
    void explicitUserStrokeEndRequest() override;

    bool selectAll() override;
    void deselect() override;

    // for KoPathToolSelection
    void notifyPathPointsChanged(KoPathShape *shape);

public:
    void canvasResourceChanged(int key, const PkVariant & res) override;

private:
    struct PathSegment;

    PathSegment* segmentAtPoint(const PkPointF &point);

private:
    void pointTypeChangedCorner();
    void pointTypeChangedSmooth();
    void pointTypeChangedSymmetric();
    void pointTypeChanged(KoPathPointTypeCommand::PointType type);
    void insertPoints();
    void removePoints();
    void segmentToLine();
    void segmentToCurve();
    void convertToPath();
    void joinPoints();
    void mergePoints();
    void breakAtPoint();
    void breakAtSegment();
    void breakAtSelection();
    void pointSelectionChanged();
    void updateActions();
    void pointToLine();
    void pointToCurve();
    void slotSelectionChanged();

private:
    void clearActivePointSelectionReferences();
    void initializeWithShapes(const PkList<KoShape*> shapes);
    KUndo2Command* createPointToCurveCommand(const PkList<KoPathPointData> &points);
    void mergePointsImpl(bool doJoin);

protected:
    KoPathToolSelection m_pointSelection; ///< the point selection
    KisCanvasCursorToken m_selectCursor;

private:
    PkScopedPointer<KoPathToolHandle> m_activeHandle;       ///< the currently active handle
    PkPointF m_lastPoint; ///< needed for interaction strategy
    PkScopedPointer<PathSegment> m_activeSegment;

    // make a friend so that it can test private member/methods
    friend class TestPathTool;

    PkScopedPointer<KoInteractionStrategy> m_currentStrategy; ///< the rubber selection strategy

    QAction *m_actionPathPointCorner;
    QAction *m_actionPathPointSmooth;
    QAction *m_actionPathPointSymmetric;
    QAction *m_actionCurvePoint;
    QAction *m_actionLinePoint;
    QAction *m_actionLineSegment;
    QAction *m_actionCurveSegment;
    QAction *m_actionAddPoint;
    QAction *m_actionRemovePoint;
    QAction *m_actionBreakPoint;
    QAction *m_actionBreakSegment;
    QAction *m_actionBreakSelection;
    QAction *m_actionJoinSegment;
    QAction *m_actionMergePoints;
    QAction *m_actionConvertToPath;
    KisCanvasCursorToken m_moveCursor;
    PkScopedPointer<KoSvgTextShapeOutlineHelper> m_textOutlineHelper;
    KisSignalAutoConnectionsStore m_canvasConnections;
    PkList<PkConnection> m_actionConnections;
    KoShapeFillResourceConnector m_shapeFillResourceConnector;

    PK_DECLARE_PRIVATE(KoToolBase)
};

#endif
