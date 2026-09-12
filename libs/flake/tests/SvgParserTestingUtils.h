/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SVGPARSERTESTINGUTILS_H
#define SVGPARSERTESTINGUTILS_H

#include <svg/SvgParser.h>
#include <kis_debug.h>
#include <kis_global.h>
#include <KoViewConverter.h>
#include <KoDocumentResourceManager.h>
#include <KoShape.h>
#include <KoShapeGroup.h>
// 渲染面已迁 Pk：KoShapePainter 的形参是 PkPainter&（libs/flake/svg/KoShapePainter.h:46），
// 对应地把「画到 QImage 上」换成「画到 PkImage 上」——
// PkPainter + PkImageRasterBackend 这一对是已跑通的现成形制
// （先例：pk/render/oracle/shape_primitive_cases.h:121 → PkImageRasterBackend backend(image);
//          pk/render/oracle/blur_kernel_cases.h:191 同形）。
// 本头是 qt 桶（消费方 TestSvgParser*/TestSvgText*/TestXsimdPainting 都带 -DQT_CORE_LIB、
// 无 pk/*/compat），所以这里补的是真 Pk 头（PkPainter/PkImageRasterBackend），
// 而不是把 pk/*/compat 拉进来（IMPACT §1 的桶纪律）。
#include <PkPainter.h>
#include <PkImageRasterBackend.h>
#include <KoShapePainter.h>

#include "kis_algebra_2d.h"

#include <QXmlSimpleReader>

struct SvgTester
{
    SvgTester (const PkString &data)
        : doc(SvgParser::createDocumentFromSvg(data)),
          m_parser(new SvgParser(&resourceManager))
    {
        root = doc.documentElement();

        parser().setXmlBaseDir("./");


        savedData = data;
        //printf("%s", savedData.toUtf8().data());

    }

    ~SvgTester ()
    {
        qDeleteAll(shapes);
    }

    void run() {
        shapes = parser().parseSvg(root, &fragmentSize);
    }

    KoShape* findShape(const PkString &name, KoShape *parent = 0) {
        if (parent && parent->name() == name) {
            return parent;
        }

        PkList<KoShape*> children;

        if (!parent) {
            children = shapes;
        } else {
            KoShapeContainer *cont = dynamic_cast<KoShapeContainer*>(parent);
            if (cont) {
                children = cont->shapes();
            }
        }

        Q_FOREACH (KoShape *shape, children) {
            KoShape *result = findShape(name, shape);
            if (result) {
                return result;
            }
        }

        return 0;
    }

    KoShapeGroup* findGroup(const PkString &name) {
        KoShapeGroup *group = 0;
        KoShape *shape = findShape(name);
        if (shape) {
            group = dynamic_cast<KoShapeGroup*>(shape);
        }
        return group;
    }

    SvgParser& parser() {
        return *m_parser;
    }

    KoDocumentResourceManager resourceManager;
    PkXmlDocument doc;
    PkXmlElement root;
    PkSizeF fragmentSize;
    PkList<KoShape*> shapes;
    PkString savedData;

protected:
    PkScopedPointer<SvgParser> m_parser;
};

#include <qimage_test_util.h>
// PkImage→QImage / PkString→QString 的桥：用 **libs/flake 自己的** PkFlakeBridge
// （toQImage:PkFlakeBridge.h:533、toQString:PkFlakeBridge.h:100），不是
// TestUtil::diagnosticQImage —— 后者住在 testutil.h，而 testutil.h:40
// `#include <kis_paint_device.h>` 让它的 include 链伸进 libs/image，
// 本头的 7 个消费 target 只链 kritaflake+kritatestsdk，够不着（实测
// `sdk/tests/testutil.h:40:10: fatal error: 'kis_paint_device.h' file not found`）。
// PkFlakeBridge.h 同一个 TU 里已经 include（TestSvgParser.cpp:13），零新增 include 面。
// checkQImageImpl 的形参仍是 const QImage&/const QString&（qimage_test_util.h:188），
// 只在调用点做边界转换，比较强度不变。
#include <PkFlakeBridge.h>

#ifdef USE_ROUND_TRIP
#include "SvgWriter.h"
#include <PkMemoryStream.h>
#include <PkXmlDocument.h>
#endif

struct SvgRenderTester : public SvgTester
{
    SvgRenderTester(const PkString &data)
        : SvgTester(data),
          m_fuzzyThreshold(0)
    {
    }

    void setFuzzyThreshold(int fuzzyThreshold) {
        m_fuzzyThreshold = fuzzyThreshold;
    }

    void setCheckQImagePremultiplied(bool value) {
        m_checkQImagePremultiplied = value;
    }

    static void testRender(KoShape *shape, const PkString &prefix, const PkString &testName, const PkSize canvasSize, qreal dpi, int fuzzyThreshold = 0, bool checkQImagePremultiplied = false) {
        PkImage canvas(canvasSize, PkImage::Format_ARGB32);

        // 【产品头缺口，不在本任务范围，见报告】PkImage 没有 DotsPerMeterX/Y
        // （pk/image/PkImage.h 全类无此 API，全树只有本文件与 TestSvgText.cpp:1325 两处调用）。
        // Qt 侧那两行是给 QPainter 的字体栅格化提供设备分辨率（QImage::logicalDpiY()）；
        // Pk 侧的 PkPainterBackend 只有 devicePixelRatio()（pk/render/PkPainter.h:25），
        // 没有 dpi 概念，所以 dpi 参数在渲染面上没有对应表达。
        // 这里如实丢弃，不编造等价物；差异（Pk 侧不按 dpi 缩放字形）属产品面，写进报告。
        Q_UNUSED(dpi);

        canvas.fill(0);

        // PkPainter 需要一个后端；PkImage 目的地的后端是 PkImageRasterBackend
        // （libs/flake/PkImageRasterBackend.h:11）。backend 必须先于 painter 构造，
        // 且都活到 paint 之后（painter 持有 backend 引用）。
        PkImageRasterBackend backend(canvas);
        PkPainter painter(backend);

        KoShapePainter p;
        p.setShapes({shape});
        painter.setClipRect(canvas.rect());
        p.paint(painter);

        // 边界转换：PkImage→QImage、PkString→QString（PkFlakeBridge 的 toQImage/toQString）；
        // checkQImageImpl 的比较强度、容差与 fuzzy 语义逐字不变（qimage_test_util.h:188
        // 的形参一个没动，fuzzyAlpha 仍传 -1、maxNumFailingPixels 仍传 0）。
        QVERIFY(TestUtil::checkQImageImpl(false,
                                          toQImage(canvas), "svg_render",
                                          toQString(prefix),
                                          toQString(testName),
                                          fuzzyThreshold, -1, 0,
                                          checkQImagePremultiplied));
    }

    void test_standard_30px_72ppi(const PkString &testName, bool verifyGeometry = true, const PkSize &canvasSize = PkSize(30,30)) {
        test_standard_impl(testName, verifyGeometry, canvasSize, 72.0);
    }

    void test_standard(const PkString &testName, const PkSize &canvasSize, qreal pixelsPerInch) {
        test_standard_impl(testName, false, canvasSize, pixelsPerInch);
    }

    void test_standard_impl(const PkString &testName, bool verifyGeometry, const PkSize &canvasSize, qreal pixelsPerInch) {

        PkSize sizeInPx = canvasSize;
        PkSizeF sizeInPt = PkSizeF(canvasSize) * 72.0 / pixelsPerInch;
        Q_UNUSED(sizeInPt); // used in some definitions only!


        parser().setResolution(PkRectF(PkPointF(), sizeInPx) /* px */, pixelsPerInch /* ppi */);
        run();

#ifdef USE_CLONED_SHAPES
        {
            PkList<KoShape*> newShapes;
            Q_FOREACH (KoShape *shape, shapes) {
                KoShape *clonedShape = shape->cloneShape();
                KIS_ASSERT(clonedShape);

                newShapes << clonedShape;
            }

            qDeleteAll(shapes);
            shapes = newShapes;
        }

#endif /* USE_CLONED_SHAPES */

#ifdef USE_ROUND_TRIP

        PkMemoryStream writeBuf;
        writeBuf.open(PkStream::WriteOnly);

        {
            SvgWriter writer(shapes);
            writer.save(writeBuf, sizeInPt);
        }

        PkXmlDocument prettyDoc;
        prettyDoc.setContent(savedData);


        qDebug();
        // PkString 没有 data()（pk/string/PkString.h:136/141 给的是 utf16() 与 PkToUtf8()），
        // PkXmlDocument::toByteArray 返 PkString（pk/xml/PkXmlDocument.h:88，已知偏离——
        // 原本该返 PkByteArray，R-02 未交付）。两条都沿用 PkToUtf8() 取 UTF-8 字节。
        printf("\n=== Original: ===\n\n%s\n", prettyDoc.toByteArray(4).PkToUtf8().c_str());
        // PkMemoryStream::data() 本身就是 const char*（libs/store/PkMemoryStream.h:28），
        // 不再有 Qt 那层 QByteArray 中转，所以只取一次 .data()。
        printf("\n=== Saved: ===\n\n%s\n", writeBuf.data());
        qDebug();

        // setContent(const PkString&, ...) 与 setContent(PkStream*, ...) 之间需要显式消歧：
        // 传 const char* 时两个重载的实参转换都成立，clang 报 ambiguous。
        // 这里是「把内存流里刚写出的字节当 XML 文本解析」，所以显式构造 PkString 走文本重载。
        QVERIFY(doc.setContent(PkString(writeBuf.data())));
        root = doc.documentElement();

        // reset the parser to avoid name conflicts

        m_parser.reset(new SvgParser(&resourceManager));
        parser().setResolution(PkRectF(PkPointF(), sizeInPx) /* px */, pixelsPerInch /* ppi */);

        run();
#endif /* USE_ROUND_TRIP */

        KoShape *shape = findShape("testRect");
        KIS_ASSERT(shape);

        if (verifyGeometry) {
            QCOMPARE(shape->absolutePosition(KoFlake::TopLeft), PkPointF(5,5));

            const PkPointF bottomRight= shape->absolutePosition(KoFlake::BottomRight);
            const PkPointF expectedBottomRight(15,25);

            if (KisAlgebra2D::norm(bottomRight - expectedBottomRight) > 0.0001 ) {
                QCOMPARE(bottomRight, expectedBottomRight);
            }
        }

        testRender(shape, "load", testName, canvasSize, pixelsPerInch, m_fuzzyThreshold, m_checkQImagePremultiplied);
    }

private:
    int m_fuzzyThreshold;
    int m_checkQImagePremultiplied = false;
};


#endif // SVGPARSERTESTINGUTILS_H
