/*
 * SPDX-FileCopyrightText: 2026 S-09-g
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QTest>

#include "KoCanvasBase.h"
#include "KoShapeControllerBase.h"
#include "KoUnit.h"

#include <PkThread.h>
#include <PkThreadCallQueue.h>

#include <future>
#include <thread>

namespace
{
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
}

class KoCanvasDeferredCallTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void initTestCase()
    {
        PkThreadCallQueue::warmUpCurrentThread();
    }

    void cleanup()
    {
        PkThreadCallQueue::processPendingCalls();
    }

    void postWithoutPumpDoesNotExecute()
    {
        MinimalShapeController shapeController;
        MinimalCanvas canvas(&shapeController);
        int calls = 0;

        canvas.postDeferredCall([&calls] { ++calls; });

        QCOMPARE(calls, 0);
        QCOMPARE(PkThreadCallQueue::pendingCount(), std::size_t(1));
    }

    void explicitPumpExecutesOnceOnCanvasThread()
    {
        struct Result {
            int dequeued = 0;
            int calls = 0;
            bool ranOnTarget = false;
            int secondPump = 0;
            PkThreadId targetThread {};
        };
        struct Target {
            MinimalCanvas *canvas = nullptr;
            Result *result = nullptr;
        };

        std::promise<Target> canvasReady;
        std::future<Target> canvasFuture = canvasReady.get_future();
        std::promise<void> callPosted;
        std::future<void> callPostedFuture = callPosted.get_future();
        std::promise<Result> completed;
        std::future<Result> completedFuture = completed.get_future();

        std::thread targetThread([&] {
            MinimalShapeController shapeController;
            MinimalCanvas canvas(&shapeController);
            Result result;
            result.targetThread = PkThreadCallQueue::warmUpCurrentThread();

            canvasReady.set_value(Target {&canvas, &result});
            callPostedFuture.wait();
            result.dequeued = PkThreadCallQueue::processPendingCalls();
            result.secondPump = PkThreadCallQueue::processPendingCalls();
            completed.set_value(result);
        });

        const Target target = canvasFuture.get();
        target.canvas->postDeferredCall([result = target.result] {
            ++result->calls;
            result->ranOnTarget = PkThread::currentThreadId() == result->targetThread;
        });
        callPosted.set_value();

        Result result = completedFuture.get();
        targetThread.join();

        QCOMPARE(result.dequeued, 1);
        QCOMPARE(result.calls, 1);
        QVERIFY(result.ranOnTarget);
        QCOMPARE(result.secondPump, 0);
    }

    void canvasTeardownBeforePumpSuppressesCallback()
    {
        MinimalShapeController shapeController;
        int calls = 0;
        auto *canvas = new MinimalCanvas(&shapeController);
        canvas->postDeferredCall([&calls] { ++calls; });

        delete canvas;

        QCOMPARE(PkThreadCallQueue::processPendingCalls(), 1);
        QCOMPARE(calls, 0);
    }

    void repeatedTransientUsersKeepPersistentOwnerAlive()
    {
        MinimalShapeController shapeController;
        MinimalCanvas canvas(&shapeController);
        int calls = 0;

        canvas.postDeferredCall([&calls] { ++calls; });
        QCOMPARE(PkThreadCallQueue::processPendingCalls(), 1);
        canvas.postDeferredCall([&calls] { ++calls; });
        QCOMPARE(PkThreadCallQueue::processPendingCalls(), 1);

        QCOMPARE(calls, 2);
    }
};

QTEST_GUILESS_MAIN(KoCanvasDeferredCallTest)

#include "KoCanvasDeferredCallTest.moc"
