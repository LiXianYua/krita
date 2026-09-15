/*
 *  SPDX-FileCopyrightText: 2021 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisColorsmudgeOpTest.h"

// 本测试**不进 Pk 测试栈**，走 sdk/tests/simpletest.h 的 SIMPLE_TEST_MAIN（Qt 分支）。
// 理由：它经 qimage_based_test.h 依赖 sdk/tests/testutil.h，而后者已被 S-06 Task 9
// 登记为 [GAP]——保留 Qt 类型（QImage 文件 I/O + TestNode 的 Q_OBJECT），关闭条件是
// R-15 的 PkImage 文件 I/O。testutil.h:511 有一处 PATTERN-2 的 `QTest::qWait`
// 要靠真 Qt 的 QTest。所以这个 TU 里 `QTest` 必须仍是真 Qt 的 QTest。
//
// 原先此处的 `#include <pk/test/compat/QTest>` 定义了 `#define QTest PkTest`
// （pk/test/compat/QTest:8），于是 testutil.h:511 的 `QTest::qWait` 被改写成
// `PkTest::qWait` 而报「no member named 'qWait' in namespace 'PkTest'」；
// 文件末尾又是 PK_TEST_MAIN ⇒ 还要求 PkTestBinder<KisColorsmudgeOpTest>（未生成）。
// 同目录同类测试 KisBrushOpTest（kis_brushop_test.h + .cpp）用的就是
// `<simpletest.h>` + SIMPLE_TEST_MAIN 这一形态，此处与之对齐；测试体、断言、
// QImageBasedTest 的像素比对全部不动。
// ⚠ 【声明出来的覆盖洞 —— R-85，2026-09-15 人拍板走 A：对齐上游】
// 本文件下半部分那处 checkOneLayer 调用是**裸调用**（丢弃返回值），与上游
// KDE/krita v6.0.3（同文件 :83）逐字同形。**因此本测试从此不检查任何东西。**
//
// 为什么：checkOneLayer 会比对本用例的参考图，而它要的 **16 张参考图**
// （8 个 .kpp 预设 × norm/over 两个模式）**上游与 fork 都不存在** —— 两侧
// tests/data/ 都只有 8 个 .kpp、一张 .png 都没有（2026-09-15 双向实测：fork 侧
// `find ... -name '*.png'` 为 0；上游 GitHub contents API `?ref=v6.0.3` 同样 0）。
// 参考图缺席时 QImageBasedTest::checkOneQImage（sdk/tests/qimage_based_test.h:246）
// 拿到的是**空 QImage**，裸调用下无所谓；M0 基线记的 `Passed` 只可能由这一形态解释。
//
// **不许顺手「补」参考图** —— 上游都没有的参考图 = 自己造判据，明令禁止。
// 先例（R-80）：用「复制 5 个 .asl」凑 TestAslStorage 的绿，复制让解析器第一次
// 真跑起来、撞上潜伏死循环，把那门测试从 `Failed 0.04 s` 变成 `Timeout 200 s`
// —— 比原来更坏。**凑出来的绿，代价常常在别处结算。**
//
// **这个洞是登记出来的，不是静默的。** 通则见 R线-spec〈上游与 fork 不同形、且参考
// 数据两边都不存在时：对齐上游，并把「它其实不检查」写在明处〉；本文件的改动与理由
// 见 docs/superpowers/plans/R-85.md 与 docs/TASKS.md 的 R-85 行。
// 关闭条件：上游补进参考图（那时删掉本注释并把裸调用恢复成断言），或本测试判不迁。

#define USE_DOCUMENT 0
#include <qimage_based_test.h>
#undef USE_DOCUMENT
#include <stroke_testing_utils.h>
#include <brushengine/kis_paint_information.h>
#include <KoCanvasResourceProvider.h>
#include <brushengine/kis_paintop_preset.h>
#include <brushengine/kis_paintop_settings.h>
#include <KoCanvasResourcesIds.h>
#include <PkScopedPointer.h>
#include <kis_painter.h>
#include <kis_resources_snapshot.h>
#include <string>

class TestColorsmudgeOp : public TestUtil::QImageBasedTest
{
public:
    TestColorsmudgeOp(const QString &prefix = "simple")
        : QImageBasedTest("colorsmudgeop") {
        m_prefix = prefix;
    }

    void test(const QString &testName, const QString &presetFileName, bool useOverlay) {
        KisSurrogateUndoStore *undoStore = new KisSurrogateUndoStore();
        KisImageSP image = createTrivialImage(undoStore);
        image->initialRefreshGraph();
        image->resizeImage(PkRect(0,0,200,200));
        image->waitForDone();

        KisNodeSP paint1 = findNode(image->root(), "paint1");

        QVERIFY(paint1->extent().isEmpty());

        paint1->paintDevice()->fill(PkRect(80, 5, 50, 190), KoColor(Pk::red, image->colorSpace()));

        KisNodeSP targetNode = paint1;

        if (useOverlay) {
            KisPaintLayerSP paint2 = new KisPaintLayer(image, "paint2", OPACITY_OPAQUE_U8);
            image->addNode(paint2, paint1->parent(), paint1);
            targetNode = paint2;

            KisPaintLayerSP paintBg = new KisPaintLayer(image, "paintBg", OPACITY_OPAQUE_U8);
            image->addNode(paintBg, paint1->parent(), 0);
            paintBg->paintDevice()->fill(PkRect(0, 100, 200, 100), KoColor(Pk::white, image->colorSpace()));

            image->initialRefreshGraph();
        }

        KisPainter gc(targetNode->paintDevice());

        PkScopedPointer<KoCanvasResourceProvider> manager(
            utils::createResourceManager(image, 0, TestUtil::pkStringFromQString(presetFileName)));

        manager->setResource(KoCanvasResource::ForegroundColor, KoColor(Pk::green, image->colorSpace()));

        KisPaintOpPresetSP preset =
            manager->resource(KoCanvasResource::CurrentPaintOpPreset).value<KisPaintOpPresetSP>();

        if (useOverlay) {
            preset->settings()->setProperty("MergedPaint", true);
        }

        QString testPrefix =
            QString("%1_%2")
            .arg(m_prefix)
            .arg(testName);

        KisResourcesSnapshotSP resources =
            new KisResourcesSnapshot(image,
                                     targetNode,
                                     manager->canvasResourcesInterface());

        resources->setupPainter(&gc);

        doPaint(gc);

        checkOneLayer(image, targetNode, testPrefix);
    }

    void doPaint(KisPainter &gc) {

        const QVector<qreal> pressureLevels = {1.0, 0.8, 0.5};

        int yOffset = 20;
        Q_FOREACH (qreal pressure, pressureLevels) {
            {
                KisDistanceInformation dist;
                KisPaintInformation p1(PkPointF(20, yOffset), pressure);
                KisPaintInformation p2(PkPointF(180, yOffset), pressure);

                gc.paintLine(p1, p2, &dist);
            }

            {
                KisDistanceInformation dist;
                KisPaintInformation p1(PkPointF(100, yOffset + 30), pressure);
                KisPaintInformation p2(PkPointF(180, yOffset + 30), pressure);

                gc.paintLine(p1, p2, &dist);
            }

            yOffset += 60;
        }
    }

    QString m_presetFileName;
    QString m_prefix;
};

void KisColorsmudgeOpTest::test_data()
{
    QTest::addColumn<QString>("testName");
    QTest::addColumn<QString>("preset");
    QTest::addColumn<bool>("overlay");

    QStringList files = {
        "test_smudge_20px_dul_nsa_new.0001.kpp",
        "test_smudge_20px_dul_nsa_old.0001.kpp",
        "test_smudge_20px_dul_sa_new.0001.kpp",
        "test_smudge_20px_dul_sa_old.0001.kpp",
        "test_smudge_20px_sme_nsa_new.0001.kpp",
        "test_smudge_20px_sme_nsa_old.0001.kpp",
        "test_smudge_20px_sme_sa_new.0001.kpp",
        "test_smudge_20px_sme_sa_old.0001.kpp"
    };

    for (int i = 0; i < 2; i++) {
        const bool useOverlay = bool(i);
        Q_FOREACH (const QString &file, files) {
            const int prefixLength = int(PkString("test_smudge_").size());
            const int suffixLength = int(PkString(".0001.kpp").size());
            const QString caseName = file.mid(prefixLength, file.size() - prefixLength - suffixLength);
            const QString name = QString("%1_%2").arg(useOverlay ? "over" : "norm").arg(caseName);
            const std::string rowName = name.toStdString();
            QTest::addRow("%s", rowName.c_str()) << name << file << useOverlay;
        }
    }
}

void KisColorsmudgeOpTest::test()
{
    QFETCH(QString, testName);
    QFETCH(QString, preset);
    QFETCH(bool, overlay);

    TestColorsmudgeOp t;
    t.test(testName, preset, overlay);
}

// SIMPLE_TEST_MAIN（simpletest.h）的默认分支 = QApplication + 真 QTest::qExec，
// 正是「未迁移的 QObject fixture」那一档；kis_brushop_test / kis_processings_test /
// kis_projection_leaf_test 三个 qimage_based_test.h 消费者全都在这条路径上。
SIMPLE_TEST_MAIN(KisColorsmudgeOpTest)
