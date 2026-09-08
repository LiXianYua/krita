/*
 * SPDX-FileCopyrightText: 2026 S-09-g
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QTest>

#include "KoCanvasBase.h"
#include "KoShapeControllerBase.h"
#include "KoToolBase.h"
#include "KoToolProxy.h"
#include "KoToolProxy_p.h"
#include "KoUnit.h"
#include "KoViewConverter.h"
#include "KoShapeManager.h"
#include "KoSelectedShapesProxySimple.h"
#include "KoPathShape.h"
#include "KoPointerEvent.h"
#include "tools/KoPathTool.h"
#include "tools/KoPathToolSelection.h"
#include "tools/KoPencilTool.h"

#include <vector>
#include <type_traits>

#include <PkPaintCommand.h>
#include <PkPainter.h>
#include <PkSize.h>

namespace
{
class RecordingBackend final : public PkPainterBackend
{
public:
    void submit(const PkPaintCommand &command) override
    {
        lastCommand = command;
        ++commandCount;
        commands.push_back(command);
    }

    PkPaintCommand lastCommand {PkSaveCommand {}};
    int commandCount = 0;
    std::vector<PkPaintCommand> commands;
};

class MinimalShapeController final : public KoShapeControllerBase
{
public:
    PkRectF documentRectInPixels() const override
    {
        return PkRectF(0.0, 0.0, 100.0, 100.0);
    }

    qreal pixelsPerInch() const override
    {
        return 72.0;
    }
};

class MinimalCanvas final : public KoCanvasBase
{
public:
    explicit MinimalCanvas(KoShapeControllerBase *shapeController)
        : KoCanvasBase(shapeController)
        , manager(this)
        , selectedShapes(&manager)
    {
    }

    void gridSize(PkPointF *, PkSizeF *) const override {}
    bool snapToGrid() const override { return false; }
    void setCursor(const QCursor &) override {}
    void addCommand(KUndo2Command *) override {}
    KoShapeManager *shapeManager() const override { return const_cast<KoShapeManager *>(&manager); }
    KoSelectedShapesProxy *selectedShapesProxy() const override { return const_cast<KoSelectedShapesProxySimple *>(&selectedShapes); }
    void updateCanvas(const PkRectF &) override {}
    KoToolProxy *toolProxy() const override { return nullptr; }
    const KoViewConverter *viewConverter() const override { return &converter; }
    KoViewConverter *viewConverter() override { return &converter; }
    QWidget *canvasWidget() override { return nullptr; }
    const QWidget *canvasWidget() const override { return nullptr; }
    KoUnit unit() const override { return KoUnit(KoUnit::Millimeter); }

    KoShapeManager manager;
    KoSelectedShapesProxySimple selectedShapes;
    KoViewConverter converter;
};

class TestToolProxy final : public KoToolProxy
{
public:
    explicit TestToolProxy(KoCanvasBase *canvas)
        : KoToolProxy(canvas)
    {
    }

protected:
    PkPointF widgetToDocument(const PkPointF &point) const override
    {
        return point;
    }

    PkPointF documentToWidget(const PkPointF &point) const override
    {
        return point;
    }
};

class PkOnlyTool final : public KoToolBase
{
public:
    using KoToolBase::paint;

    explicit PkOnlyTool(KoCanvasBase *canvas)
        : KoToolBase(canvas)
    {
    }

    void paint(PkPainter &painter, const KoViewConverter &) override
    {
        reached = true;
        painter.drawPoint(PkPointF(3.0, 4.0));
    }

    void mousePressEvent(KoPointerEvent *) override {}
    void mouseMoveEvent(KoPointerEvent *) override {}
    void mouseReleaseEvent(KoPointerEvent *) override {}

    bool reached = false;
};

class PencilPreviewTool final : public KoPencilTool
{
public:
    using KoPencilTool::KoPencilTool;
    using KoPencilTool::setStrokeColor;
    ~PencilPreviewTool() override { delete path(); }
};

// Taking the member address without a cast fails if a second dispatch overload returns.
static_assert(std::is_same_v<decltype(&KoToolProxy::paint),
                            void (KoToolProxy::*)(PkPainter &, const KoViewConverter &)>);
static_assert(std::is_same_v<decltype(&KoToolBase::paint),
                            void (KoToolBase::*)(PkPainter &, const KoViewConverter &)>);
}

class KoToolProxyPkPainterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void pathSelectionDecorationsUsePkDispatch()
    {
        MinimalShapeController controller;
        MinimalCanvas canvas(&controller);
        TestToolProxy proxy(&canvas);
        KoPathShape shape;
        auto *point = shape.moveTo(PkPointF(10.0, 20.0));
        shape.lineTo(PkPointF(30.0, 20.0));
        KoPathTool tool(&canvas);
        auto *selection = dynamic_cast<KoPathToolSelection *>(tool.selection());
        QVERIFY(selection);
        selection->setSelectedShapes({&shape});
        selection->add(point, false);
        proxy.priv()->activeTool = &tool;

        RecordingBackend backend;
        PkPainter painter(backend);
        proxy.paint(painter, *canvas.viewConverter());
        selection->setSelectedShapes({});
        selection->clear();

        int handles = 0;
        for (const auto &command : backend.commands) {
            if (const auto *polygon = std::get_if<PkDrawPolygonCommand>(&command)) {
                if (polygon->polygon.boundingRect().center() == PkPointF(10.0, 20.0)) {
                    ++handles;
                }
            }
        }
        QVERIFY(handles > 0);
        QCOMPARE(painter.transform(), PkTransform());
    }

    void pencilStrokePreviewUsesPkDispatch()
    {
        MinimalShapeController controller;
        MinimalCanvas canvas(&controller);
        TestToolProxy proxy(&canvas);
        PencilPreviewTool tool(&canvas);
        tool.setStrokeTemplate(KoShapeStroke(2.0, Pk::red));
        tool.setStrokeColor(Pk::red);
        QMouseEvent press(QEvent::MouseButtonPress, QPointF(10, 20),
                          Qt::LeftButton, Qt::LeftButton, Qt::NoModifier);
        KoPointerEvent start(&press, PkPointF(10, 20));
        tool.mousePressEvent(&start);
        QMouseEvent move(QEvent::MouseMove, QPointF(30, 20),
                         Qt::NoButton, Qt::LeftButton, Qt::NoModifier);
        KoPointerEvent end(&move, PkPointF(30, 20));
        tool.mouseMoveEvent(&end);
        proxy.priv()->activeTool = &tool;

        RecordingBackend backend;
        PkPainter painter(backend);
        proxy.paint(painter, *canvas.viewConverter());

        int fills = 0;
        for (const auto &command : backend.commands) {
            if (const auto *fill = std::get_if<PkFillPathCommand>(&command)) {
                QVERIFY(!fill->path.isEmpty());
                QCOMPARE(fill->brush.color(), PkColor(Pk::red));
                QVERIFY(fill->path.boundingRect().contains(PkPointF(20, 20)));
                ++fills;
            }
        }
        QCOMPARE(fills, 1);
        QCOMPARE(painter.transform(), PkTransform());
    }

    void dispatchesToPkOnlyToolOverride()
    {
        MinimalShapeController shapeController;
        MinimalCanvas canvas(&shapeController);
        TestToolProxy proxy(&canvas);
        PkOnlyTool tool(&canvas);
        proxy.priv()->activeTool = &tool;

        RecordingBackend backend;
        PkPainter painter(backend);
        KoViewConverter converter;
        proxy.paint(painter, converter);

        QVERIFY(tool.reached);
        QCOMPARE(backend.commandCount, 1);
        QVERIFY(std::holds_alternative<PkDrawPointCommand>(backend.lastCommand));
        const PkPointF point = std::get<PkDrawPointCommand>(backend.lastCommand).point;
        QCOMPARE(point, PkPointF(3.0, 4.0));
    }
};

QTEST_MAIN(KoToolProxyPkPainterTest)

#include "KoToolProxyPkPainterTest.moc"
