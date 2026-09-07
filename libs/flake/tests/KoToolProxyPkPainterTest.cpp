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
    }

    PkPaintCommand lastCommand {PkSaveCommand {}};
    int commandCount = 0;
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
    {
    }

    void gridSize(PkPointF *, PkSizeF *) const override {}
    bool snapToGrid() const override { return false; }
    void setCursor(const QCursor &) override {}
    void addCommand(KUndo2Command *) override {}
    KoShapeManager *shapeManager() const override { return nullptr; }
    KoSelectedShapesProxy *selectedShapesProxy() const override { return nullptr; }
    void updateCanvas(const PkRectF &) override {}
    KoToolProxy *toolProxy() const override { return nullptr; }
    const KoViewConverter *viewConverter() const override { return nullptr; }
    KoViewConverter *viewConverter() override { return nullptr; }
    QWidget *canvasWidget() override { return nullptr; }
    const QWidget *canvasWidget() const override { return nullptr; }
    KoUnit unit() const override { return KoUnit(KoUnit::Millimeter); }
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
}

class KoToolProxyPkPainterTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
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

QTEST_GUILESS_MAIN(KoToolProxyPkPainterTest)

#include "KoToolProxyPkPainterTest.moc"
