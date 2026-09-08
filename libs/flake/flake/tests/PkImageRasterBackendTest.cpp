/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QTest>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <KoPathShape.h>
#include <KoColorBackground.h>
#include <KoGradientBackground.h>
#include <KoShapeStroke.h>
#include <KoShapeManager.h>
#include <KoClipMaskPainter.h>
#include <KoClipMask.h>
#include <shapes/ImageShape.h>
#include <shapes/ImageShapePngData.h>
#include <QBuffer>

#include <array>
#include <cstring>
#include <random>
#include <memory>
#include <stdexcept>
#include <vector>

#include <PkImageRasterBackend.h>
#include <PkPainter.h>

class PkImageRasterBackendTest : public QObject
{
    Q_OBJECT

private Q_SLOTS:
    void blendsImageWithOpacity();
    void matchesQtArgb32SourceOverMatrix();
    void matchesQtShortSpansAndTails();
    void matchesQtPlusPixelsAndOverlappingMasks();
    void matchesQtTransformedClippedPathCoverage();
    void matchesQtLargeDisconnectedPathCoverage();
    void matchesQtStrokePixels();
    void matchesQtGradientPixels();
    void matchesQtTransformedImagePixels();
    void matchesQtPatternImagePixels();
    void matchesQtTexturePathPixels();
    void matchesQtClipQueries();
    void matchesQtNativeShapePainting();
    void matchesQtPremultipliedDestination();
    void matchesQtHighDepthImageSources();
    void preservesManagerDispatchAndMaskBuffers();
    void emptyAndDisjointMasksAreNoOps();
    void paintsDecodedGrayscale16();
    void paintsPatternBackgroundsAndThinGradientStrokes();
    void paintsGrayscaleDestination();
    void gradientCopiesKeepIndependentValues();
    void clipsToDestinationBounds();
    void rejectsUnsupportedOperations();
    void reportsDestinationDevicePixelRatio();
};

void PkImageRasterBackendTest::matchesQtLargeDisconnectedPathCoverage()
{
    QImage qtImage(320, 160, QImage::Format_ARGB32);
    PkImage pkImage(320, 160, PkImage::Format_ARGB32);
    qtImage.fill(Qt::black);
    pkImage.fill(Pk::black);

    QPainterPath qtSource;
    PkPainterPath pkSource;
    qtSource.moveTo(2, 2); pkSource.moveTo(2, 2);
    qtSource.cubicTo(30, -2, -2, 28, 25, 18);
    pkSource.cubicTo(30, -2, -2, 28, 25, 18);
    qtSource.lineTo(3, 20); pkSource.lineTo(3, 20);
    qtSource.closeSubpath(); pkSource.closeSubpath();
    QTransform qtTransform;
    PkTransform pkTransform;
    qtTransform.translate(2.25, 1.5); pkTransform.translate(2.25, 1.5);
    qtTransform.rotate(13); pkTransform.rotate(13);
    QPainterPath qtPath = qtTransform.map(qtSource);
    PkPainterPath pkPath = pkTransform.map(pkSource);
    qtPath.addRect(QRectF(0, 80, 300, 30));
    pkPath.addRect(PkRectF(0, 80, 300, 30));
    qtPath.addRect(QRectF(80000, 20, 20, 20));
    pkPath.addRect(PkRectF(80000, 20, 20, 20));

    QPainter qtPainter(&qtImage);
    PkImageRasterBackend backend(pkImage);
    PkPainter pkPainter(backend);
    qtPainter.setRenderHint(QPainter::Antialiasing, true);
    pkPainter.setRenderHint(PkPainter::Antialiasing, true);
    qtPainter.fillPath(qtPath, Qt::white);
    pkPainter.fillPath(pkPath, Pk::white);
    qtPainter.end();

    for (int y = 0; y < qtImage.height(); ++y) {
        for (int x = 0; x < qtImage.width(); ++x) {
            const QString context = QStringLiteral("x=%1 y=%2 Qt=%3 Pk=%4")
                .arg(x).arg(y)
                .arg(qtImage.pixel(x, y), 8, 16, QLatin1Char('0'))
                .arg(pkImage.pixel(x, y), 8, 16, QLatin1Char('0'));
            QVERIFY2(pkImage.pixel(x, y) == qtImage.pixel(x, y), qPrintable(context));
        }
    }

    QImage qtDistantOnly(32, 24, QImage::Format_ARGB32);
    PkImage pkDistantOnly(32, 24, PkImage::Format_ARGB32);
    qtDistantOnly.fill(Qt::black);
    pkDistantOnly.fill(Pk::black);
    QPainter qtDistantPainter(&qtDistantOnly);
    PkImageRasterBackend distantBackend(pkDistantOnly);
    PkPainter pkDistantPainter(distantBackend);
    QPainterPath qtDistantPath;
    PkPainterPath pkDistantPath;
    qtDistantPath.addRect(QRectF(80000, 20, 20, 20));
    pkDistantPath.addRect(PkRectF(80000, 20, 20, 20));
    qtDistantPainter.fillPath(qtDistantPath, Qt::white);
    pkDistantPainter.fillPath(pkDistantPath, Pk::white);
    qtDistantPainter.end();
    for (int y = 0; y < qtDistantOnly.height(); ++y) {
        for (int x = 0; x < qtDistantOnly.width(); ++x) {
            QVERIFY(pkDistantOnly.pixel(x, y) == qtDistantOnly.pixel(x, y));
        }
    }

    QImage qtCrossing(32, 24, QImage::Format_ARGB32);
    PkImage pkCrossing(32, 24, PkImage::Format_ARGB32);
    qtCrossing.fill(Qt::black);
    pkCrossing.fill(Pk::black);
    QPainter qtCrossingPainter(&qtCrossing);
    PkImageRasterBackend crossingBackend(pkCrossing);
    PkPainter pkCrossingPainter(crossingBackend);
    QPainterPath qtCrossingPath;
    PkPainterPath pkCrossingPath;
    qtCrossingPath.addRect(QRectF(-80000, 4, 160000, 10));
    pkCrossingPath.addRect(PkRectF(-80000, 4, 160000, 10));
    qtCrossingPainter.fillPath(qtCrossingPath, Qt::white);
    pkCrossingPainter.fillPath(pkCrossingPath, Pk::white);
    qtCrossingPainter.end();
    for (int y = 0; y < qtCrossing.height(); ++y) {
        for (int x = 0; x < qtCrossing.width(); ++x) {
            QVERIFY(pkCrossing.pixel(x, y) == qtCrossing.pixel(x, y));
        }
    }
}

void PkImageRasterBackendTest::paintsGrayscaleDestination()
{
    for (bool aa : {false,true}) for (bool smooth : {false,true})
    for (auto mode : {Pk::CompositionMode_SourceOver,Pk::CompositionMode_Source,Pk::CompositionMode_Plus})
    for (int kind=0;kind<10;++kind) {
        QImage expected(41,35,QImage::Format_Grayscale8);
        PkImage actual(41,35,PkImage::Format_Grayscale8);
        // Odd width exercises padded stride; initialize native storage directly.
        expected.fill(71); actual.fill(0xff474747);
        QPainter qt(&expected); PkImageRasterBackend backend(actual); PkPainter pk(backend);
        qt.setCompositionMode(QPainter::CompositionMode(int(mode))); pk.setCompositionMode(mode);
        qt.setOpacity(.7); pk.setOpacity(.7);
        qt.setRenderHint(QPainter::Antialiasing,aa); pk.setRenderHint(PkPainter::Antialiasing,aa);
        qt.setRenderHint(QPainter::SmoothPixmapTransform,smooth); pk.setRenderHint(PkPainter::SmoothPixmapTransform,smooth);
        qt.setClipRect(QRectF(2,3,35,29)); pk.setClipRect(PkRectF(2,3,35,29));
        qt.translate(4,3); pk.translate(4,3); qt.rotate(13); pk.rotate(13);
        QPainterPath qp; PkPainterPath pp;
        qp.addRect(QRectF(2,3,27,23)); pp.addRect(PkRectF(2,3,27,23));
        QLinearGradient qg(0,0,30,20); auto pg=PkGradient::linear(PkPointF(),PkPointF(30,20));
        qg.setColorAt(0,QColor(170,50,130,180)); pg.setColorAt(0,PkColor(170,50,130,180));
        qg.setColorAt(1,QColor(20,180,90,100)); pg.setColorAt(1,PkColor(20,180,90,100));
        for (int draw=0;draw<3;++draw) {
            if (kind<5) {
                QBrush qb=kind==1||kind==4?QBrush(qg):QBrush(QColor(170,50,130,180),kind==2?Qt::DiagCrossPattern:Qt::SolidPattern);
                PkBrush pb=kind==1||kind==4?PkBrush(pg):PkBrush(PkColor(170,50,130,180));
                if (kind==2) pb.setStyle(Pk::DiagCrossPattern);
                if (kind>=3) { qt.strokePath(qp,QPen(qb,1)); pk.strokePath(pp,PkPen(pb,1)); }
                else { qt.fillPath(qp,qb); pk.fillPath(pp,pb); }
            } else {
                const auto qformat=kind==5?QImage::Format_ARGB32:kind==6?QImage::Format_ARGB32_Premultiplied:
                    kind==7?QImage::Format_RGBA64:kind==8?QImage::Format_Grayscale16:QImage::Format_RGB32;
                QImage source(9,7,qformat);
                PkImage native(9,7,PkImage::Format(int(qformat)));
                for (int y=0;y<7;++y) for (int x=0;x<9;++x)
                    source.setPixelColor(x,y,QColor::fromRgba64((x*7919+y*1471)%65536,(x*1237+y*7979)%65536,
                        (x*4793+y*4093)%65536,3000+x*6001+y*1009));
                for (int y=0;y<7;++y) std::memcpy(native.scanLine(y),source.constScanLine(y),source.bytesPerLine());
                qt.drawImage(QRectF(1.2,2.4,23,19),source); pk.drawImage(PkRectF(1.2,2.4,23,19),native);
            }
            qt.translate(1,2); pk.translate(1,2);
        }
        qt.end();
        for (int y=0;y<35;++y) for (int x=0;x<41;++x)
            QVERIFY2(actual.constScanLine(y)[x]==expected.constScanLine(y)[x],qPrintable(QString("aa=%1 smooth=%2 mode=%3 kind=%4 x=%5 y=%6 Qt=%7 native=%8")
                .arg(aa).arg(smooth).arg(int(mode)).arg(kind).arg(x).arg(y).arg(expected.constScanLine(y)[x]).arg(actual.constScanLine(y)[x])));
    }
}

void PkImageRasterBackendTest::paintsPatternBackgroundsAndThinGradientStrokes()
{
    for (bool pm : {false,true}) for (bool aa : {false,true}) for (bool scaled : {false,true}) for (bool rotated : {false,true})
    for (int kind=0;kind<16;++kind) {
        QImage expected(41,35,pm?QImage::Format_ARGB32_Premultiplied:QImage::Format_ARGB32);
        PkImage actual(41,35,pm?PkImage::Format_ARGB32_Premultiplied:PkImage::Format_ARGB32);
        expected.fill(0xff345678); actual.fill(0xff345678);
        QPainter qt(&expected); PkImageRasterBackend backend(actual); PkPainter pk(backend);
        qt.setPen(Qt::NoPen); pk.setPen(Pk::NoPen); qt.setOpacity(.7); pk.setOpacity(.7);
        qt.setRenderHint(QPainter::Antialiasing,aa); pk.setRenderHint(PkPainter::Antialiasing,aa);
        qt.translate(4,3); pk.translate(4,3);
        if (rotated) { qt.rotate(17); pk.rotate(17); }
        if (scaled) { qt.scale(.4,.4); pk.scale(.4,.4); }
        QPainterPath qp; PkPainterPath pp;
        qp.addRect(QRectF(2,3,27,23)); pp.addRect(PkRectF(2,3,27,23));
        if (kind<13) {
            qt.setBrush(QBrush(QColor(170,50,130,180),Qt::BrushStyle(int(Qt::Dense1Pattern)+kind)));
            qt.drawPath(qp);
            KoColorBackground background(PkColor(170,50,130,180),Pk::BrushStyle(int(Pk::Dense1Pattern)+kind));
            background.paint(pk,pp);
        } else {
            QLinearGradient ql(0,0,30,20); QRadialGradient qr(15,15,20); QConicalGradient qc(15,15,25);
            QGradient qg=kind==13?QGradient(ql):kind==14?QGradient(qr):QGradient(qc);
            auto pg=kind==13?PkGradient::linear(PkPointF(),PkPointF(30,20)):
                kind==14?PkGradient::radial(PkPointF(15,15),20,PkPointF(15,15)):PkGradient::conical(PkPointF(15,15),25);
            qg.setColorAt(0,QColor(170,50,130,180)); pg.setColorAt(0,PkColor(170,50,130,180));
            qg.setColorAt(1,QColor(20,180,90,100)); pg.setColorAt(1,PkColor(20,180,90,100));
            const double width=scaled?2:1;
            QPen pen(QBrush(qg),width); pen.setJoinStyle(Qt::MiterJoin);
            // ImageShape is deliberately not a KoPathShape: its stroke goes
            // through KoShapeStroke::paintBorder -> native strokePath.
            ImageShape shape; shape.setSize(PkSizeF(27,23));
            KoShapeStroke stroke(width); stroke.setLineBrush(PkBrush(pg));
            QPainterPath outline; outline.addRect(QRectF(0,0,27,23));
            qt.strokePath(outline,pen); stroke.paint(&shape,pk);
            // Retracing a path must composite overlapping spans in order.
            qp.addRect(QRectF(2,3,27,23)); pp.addRect(PkRectF(2,3,27,23));
            qt.strokePath(qp,pen); pk.strokePath(pp,stroke.resultLinePen());
        }
        qt.end();
        for (int y=0;y<35;++y) for (int x=0;x<41;++x)
            QVERIFY2(actual.pixel(x,y)==expected.pixel(x,y),qPrintable(QString("pm=%1 aa=%2 scaled=%3 kind=%4 x=%5 y=%6 Qt=%7 native=%8")
                .arg(pm).arg(aa).arg(scaled).arg(kind).arg(x).arg(y).arg(expected.pixel(x,y),8,16,QLatin1Char('0')).arg(actual.pixel(x,y),8,16,QLatin1Char('0'))));
    }
}

void PkImageRasterBackendTest::paintsDecodedGrayscale16()
{
    QImage input(9,7,QImage::Format_Grayscale16);
    for (int y=0;y<7;++y) for (int x=0;x<9;++x)
        reinterpret_cast<quint16*>(input.scanLine(y))[x]=(x*7011+y*9021)%65536;
    QByteArray bytes; QBuffer buffer(&bytes); QVERIFY(buffer.open(QIODevice::WriteOnly));
    QVERIFY(input.save(&buffer,"PNG"));
    const QImage source=QImage::fromData(bytes);
    const PkImage decoded=ImageShapePngData::decodePng(PkByteArray(bytes.constData(),bytes.size()));
    QCOMPARE(decoded.format(),PkImage::Format_Grayscale16);
    for (bool pm : {false,true}) for (bool smooth : {false,true}) for (bool shapeEntry : {false,true}) {
        QImage expected(31,25,pm?QImage::Format_ARGB32_Premultiplied:QImage::Format_ARGB32);
        PkImage actual(31,25,pm?PkImage::Format_ARGB32_Premultiplied:PkImage::Format_ARGB32);
        expected.fill(0xff234567); actual.fill(0xff234567);
        QPainter qt(&expected); PkImageRasterBackend backend(actual); PkPainter pk(backend);
        qt.translate(3,2); pk.translate(3,2); qt.setOpacity(.7); pk.setOpacity(.7);
        qt.setRenderHint(QPainter::SmoothPixmapTransform,smooth || shapeEntry);
        pk.setRenderHint(PkPainter::SmoothPixmapTransform,smooth);
        if (shapeEntry) {
            qt.setClipRect(QRectF(0,0,21,19),Qt::IntersectClip);
            qt.scale(2.1,2.3); qt.drawImage(QPoint(),source);
            ImageShape shape; shape.setSize(PkSizeF(21,19)); shape.setImage(decoded);
            shape.setViewBoxTransform(PkTransform::fromScale(2.1,2.3)); shape.paint(pk);
        } else {
            qt.drawImage(QRectF(1.2,2.4,23,19),source);
            pk.drawImage(PkRectF(1.2,2.4,23,19),decoded);
        }
        qt.end();
        for (int y=0;y<25;++y) for (int x=0;x<31;++x)
            QVERIFY2(actual.pixel(x,y)==expected.pixel(x,y),qPrintable(QString("pm=%1 smooth=%2 shape=%3 x=%4 y=%5 Qt=%6 native=%7")
                .arg(pm).arg(smooth).arg(shapeEntry).arg(x).arg(y).arg(expected.pixel(x,y),8,16,QLatin1Char('0')).arg(actual.pixel(x,y),8,16,QLatin1Char('0'))));
    }
}

void PkImageRasterBackendTest::emptyAndDisjointMasksAreNoOps()
{
    QImage expected(19,17,QImage::Format_ARGB32); expected.fill(0xff345678);
    PkImage actual(19,17,PkImage::Format_ARGB32); actual.fill(0xff345678);
    QImage empty;
    QPainter qt(&expected);
    qt.drawImage(QPoint(),empty);
    qt.setClipRect(QRect(12,12,3,3));
    qt.fillRect(QRect(1,1,4,4),Qt::red);
    qt.end();
    PkImageRasterBackend backend(actual); PkPainter pk(backend);
    KoClipMaskPainter mask(&pk,PkRectF());
    mask.shapePainter()->fillRect(PkRect(0,0,8,8),Pk::red);
    mask.maskPainter()->fillRect(PkRect(0,0,8,8),Pk::white);
    mask.renderOnGlobalPainter();
    KoPathShape shape;
    shape.moveTo(PkPointF(1,1)); shape.lineTo(PkPointF(5,1));
    shape.lineTo(PkPointF(5,5)); shape.lineTo(PkPointF(1,5)); shape.close();
    shape.setBackground(PkSharedPointer<KoColorBackground>(new KoColorBackground(Pk::red)));
    shape.setClipMask(new KoClipMask);
    pk.setClipRect(PkRect(12,12,3,3));
    KoShapeManager::renderSingleShape(&shape,pk);
    for (int y=0;y<17;++y) for (int x=0;x<19;++x)
        QCOMPARE(actual.pixel(x,y),expected.pixel(x,y));
}

void PkImageRasterBackendTest::preservesManagerDispatchAndMaskBuffers()
{
    for (bool masked : {false,true}) for (bool gray : {false,true}) {
        QImage expected(41,33,gray?QImage::Format_Grayscale8:QImage::Format_ARGB32); expected.fill(0);
        PkImage actual(41,33,gray?PkImage::Format_Grayscale8:PkImage::Format_ARGB32); actual.fill(0);
        QPainter qt(&expected); PkImageRasterBackend backend(actual); PkPainter pk(backend);
        pk.setPen(Pk::NoPen);
        qt.translate(5,4); pk.translate(5,4);
        qt.setClipRect(QRect(0,0,27,24)); pk.setClipRect(PkRect(0,0,27,24));
        QPainterPath path; path.addRect(QRectF(2,3,20,15));
        KoPathShape shape;
        shape.moveTo(PkPointF(2,3)); shape.lineTo(PkPointF(22,3));
        shape.lineTo(PkPointF(22,18)); shape.lineTo(PkPointF(2,18)); shape.close();
        shape.setBackground(PkSharedPointer<KoColorBackground>(new KoColorBackground(PkColor(150,70,190,180))));
        if (masked) {
            qt.setClipRect(QRect(7,5,11,16),Qt::IntersectClip);
            KoClipMaskPainter mask(&pk,PkRectF(5,4,27,24));
            KoShapeManager::renderSingleShape(&shape,*mask.shapePainter());
            mask.maskPainter()->fillRect(PkRect(7,5,11,16),PkColor(255,255,255));
            mask.renderOnGlobalPainter();
        } else {
            KoShapeManager::renderSingleShape(&shape,pk);
        }
        qt.fillPath(path,QColor(150,70,190,180)); qt.end();
        for (int y=0;y<33;++y) for (int x=0;x<41;++x)
            QVERIFY2(actual.pixel(x,y)==expected.pixel(x,y),qPrintable(QString("mask=%1 x=%2 y=%3 Qt=%4 native=%5")
                .arg(masked).arg(x).arg(y).arg(expected.pixel(x,y),8,16,QLatin1Char('0')).arg(actual.pixel(x,y),8,16,QLatin1Char('0'))));
        QCOMPARE(pk.transform(),PkTransform::fromTranslate(5,4));
    }
}

void PkImageRasterBackendTest::gradientCopiesKeepIndependentValues()
{
    auto gradient=PkGradient::linear(PkPointF(),PkPointF(1,1));
    gradient.setColorAt(0,PkColor(20,80,140));
    KoGradientBackground original(gradient);
    KoGradientBackground copied(original);
    auto changed=gradient;
    changed.setColorAt(0,PkColor(190,30,60));
    copied.setGradient(changed);
    QCOMPARE(original.gradient()->stops()[0].color.rgba(),PkColor(20,80,140).rgba());
    QCOMPARE(copied.gradient()->stops()[0].color.rgba(),PkColor(190,30,60).rgba());
    KoGradientBackground assigned(changed);
    assigned=original;
    assigned.setTransform(PkTransform::fromScale(2,3));
    assigned.setGradient(changed);
    QCOMPARE(original.gradient()->stops()[0].color.rgba(),PkColor(20,80,140).rgba());
    QCOMPARE(original.transform(),PkTransform());
}

void PkImageRasterBackendTest::matchesQtPremultipliedDestination()
{
    // Symbol previews render onto PM buffers. Treating their stored channels
    // as straight alpha (or unpremultiplying on store) corrupts translucent edges.
    for (int kind = 0; kind < 6; ++kind)
    for (int mode = 0; mode < 3; ++mode) for (bool aa : {false, true}) {
        QImage expected(37, 29, QImage::Format_ARGB32_Premultiplied);
        PkImage actual(37, 29, PkImage::Format_ARGB32_Premultiplied);
        expected.fill(0x70502010); actual.fill(0x70502010);
        QPainter qt(&expected); PkImageRasterBackend backend(actual); PkPainter pk(backend);
        const QPainter::CompositionMode qm[] = {QPainter::CompositionMode_SourceOver,QPainter::CompositionMode_Source,QPainter::CompositionMode_Plus};
        const Pk::CompositionMode pm[] = {Pk::CompositionMode_SourceOver,Pk::CompositionMode_Source,Pk::CompositionMode_Plus};
        qt.setCompositionMode(qm[mode]); pk.setCompositionMode(pm[mode]);
        qt.setRenderHint(QPainter::Antialiasing,aa); pk.setRenderHint(PkPainter::Antialiasing,aa);
        qt.setOpacity(.7); pk.setOpacity(.7);
        QPainterPath qp; PkPainterPath pp;
        qp.moveTo(2,3); pp.moveTo(2,3);
        qp.cubicTo(4,24,23,-5,33,24); pp.cubicTo(4,24,23,-5,33,24);
        qp.lineTo(3,25); pp.lineTo(3,25);
        qp.closeSubpath(); pp.closeSubpath();
        if (kind == 0) {
            qt.fillPath(qp,QColor(160,40,120,180)); pk.fillPath(pp,PkColor(160,40,120,180));
        } else if (kind == 1) {
            QLinearGradient qg(0,0,30,20);
            auto pg=PkGradient::linear(PkPointF(),PkPointF(30,20));
            qg.setColorAt(0,QColor(160,40,120,180)); pg.setColorAt(0,PkColor(160,40,120,180));
            qg.setColorAt(1,QColor(30,200,50,90)); pg.setColorAt(1,PkColor(30,200,50,90));
            qt.fillPath(qp,QBrush(qg)); pk.fillPath(pp,PkBrush(pg));
        } else {
            qt.setRenderHint(QPainter::SmoothPixmapTransform,kind>=3);
            pk.setRenderHint(PkPainter::SmoothPixmapTransform,kind>=3);
            QImage qi(13,9,kind>=4?QImage::Format_ARGB32_Premultiplied:QImage::Format_ARGB32);
            PkImage pi(13,9,kind>=4?PkImage::Format_ARGB32_Premultiplied:PkImage::Format_ARGB32);
            for (int y=0;y<9;++y) for (int x=0;x<13;++x) {
                const unsigned color=qRgba(x*17,y*27,150,100+x*9);
                const unsigned value=kind>=4?qPremultiply(color):color;
                qi.setPixel(x,y,value); pi.setPixel(x,y,value);
            }
            qt.setClipPath(qp); pk.setClipPath(pp);
            if (kind==5) { qt.rotate(17); pk.rotate(17); }
            qt.drawImage(QRectF(2,3,26,18),qi); pk.drawImage(PkRectF(2,3,26,18),pi);
        }
        qt.end();
        for (int y=0;y<29;++y) for (int x=0;x<37;++x)
            QVERIFY2(actual.pixel(x,y)==expected.pixel(x,y),qPrintable(QString("mode=%1 aa=%2 x=%3 y=%4 Qt=%5 native=%6 kind=%7")
                .arg(mode).arg(aa).arg(x).arg(y).arg(expected.pixel(x,y),8,16,QLatin1Char('0')).arg(actual.pixel(x,y),8,16,QLatin1Char('0')).arg(kind)));
    }
}

void PkImageRasterBackendTest::matchesQtHighDepthImageSources()
{
    // Reference images can retain 16-bit samples; the painter must not throw
    // or discard their low bits before filtering/composition.
    for (bool pm : {false,true}) for (bool smooth : {false,true})
    for (auto format : {QImage::Format_RGB32,QImage::Format_RGBX64,QImage::Format_RGBA64,QImage::Format_RGBA64_Premultiplied}) {
        QImage source(9,7,format);
        for (int y=0;y<7;++y) for (int x=0;x<9;++x)
            source.setPixelColor(x,y,QColor::fromRgba64(x*7011,y*9021,(x*3111+y*4713)%65536,1000+x*6011+y*901));
        const auto nativeFormat=format==QImage::Format_RGB32?PkImage::Format_RGB32:
            format==QImage::Format_RGBX64?PkImage::Format_RGBX64:
            format==QImage::Format_RGBA64?PkImage::Format_RGBA64:PkImage::Format_RGBA64_Premultiplied;
        PkImage nativeSource(9,7,nativeFormat);
        for (int y=0;y<7;++y) std::memcpy(nativeSource.scanLine(y),source.constScanLine(y),9*(source.depth()/8));
        QImage expected(31,25,pm?QImage::Format_ARGB32_Premultiplied:QImage::Format_ARGB32);
        PkImage actual(31,25,pm?PkImage::Format_ARGB32_Premultiplied:PkImage::Format_ARGB32);
        expected.fill(0x70502010); actual.fill(0x70502010);
        QPainter qt(&expected); PkImageRasterBackend backend(actual); PkPainter pk(backend);
        qt.setOpacity(.7); pk.setOpacity(.7);
        qt.setRenderHint(QPainter::SmoothPixmapTransform,smooth); pk.setRenderHint(PkPainter::SmoothPixmapTransform,smooth);
        qt.drawImage(QRectF(2,3,21,17),source); pk.drawImage(PkRectF(2,3,21,17),nativeSource);
        qt.end();
        for (int y=0;y<25;++y) for (int x=0;x<31;++x)
            QVERIFY2(actual.pixel(x,y)==expected.pixel(x,y),qPrintable(QString("format=%1 pm=%2 smooth=%3 x=%4 y=%5 Qt=%6 native=%7")
                .arg(format).arg(pm).arg(smooth).arg(x).arg(y).arg(expected.pixel(x,y),8,16,QLatin1Char('0')).arg(actual.pixel(x,y),8,16,QLatin1Char('0'))));
    }
}

void PkImageRasterBackendTest::matchesQtNativeShapePainting()
{
    for (bool antialias : {false, true}) {
        QImage qtImage(47, 39, QImage::Format_ARGB32);
        PkImage pkImage(47, 39, PkImage::Format_ARGB32);
        qtImage.fill(0); pkImage.fill(0);
        QPainterPath qtPath;
        KoPathShape shape;
        qtPath.moveTo(3, 5); shape.moveTo(PkPointF(3, 5));
        qtPath.lineTo(31, 7); shape.lineTo(PkPointF(31, 7));
        qtPath.lineTo(18, 27); shape.lineTo(PkPointF(18, 27));
        qtPath.closeSubpath(); shape.close();
        shape.setBackground(PkSharedPointer<KoShapeBackground>(new KoColorBackground(PkColor(40, 130, 210, 170))));
        shape.setStroke(KoShapeStrokeModelSP(new KoShapeStroke(3.25, PkColor(180, 30, 80, 210))));
        QPainter qtPainter(&qtImage);
        PkImageRasterBackend backend(pkImage);
        PkPainter painter(backend);
        qtPainter.setRenderHint(QPainter::Antialiasing, antialias);
        painter.setRenderHint(PkPainter::Antialiasing, antialias);
        qtPainter.translate(5, 2); painter.translate(5, 2);
        qtPainter.rotate(7); painter.rotate(7);
        qtPainter.setOpacity(0.9); painter.setOpacity(0.9);
        qtPainter.setPen(Qt::NoPen); painter.setPen(Pk::NoPen);
        qtPainter.fillPath(qtPath, QColor(40, 130, 210, 170));
        QPen qtPen(QColor(180, 30, 80, 210), 3.25);
        qtPen.setJoinStyle(Qt::MiterJoin);
        qtPainter.strokePath(qtPath, qtPen);
        shape.paint(painter);
        shape.paintStroke(painter);
        qtPainter.end();
        for (int y = 0; y < 39; ++y) for (int x = 0; x < 47; ++x) {
            const QString context = QStringLiteral("aa=%1 x=%2 y=%3 Qt=%4 Pk=%5")
                .arg(antialias).arg(x).arg(y)
                .arg(qtImage.pixel(x,y), 8, 16, QLatin1Char('0'))
                .arg(pkImage.pixel(x,y), 8, 16, QLatin1Char('0'));
            QVERIFY2(pkImage.pixel(x,y) == qtImage.pixel(x,y), qPrintable(context));
        }
    }
}

void PkImageRasterBackendTest::matchesQtClipQueries()
{
    // A clip is anchored at the transform active when it is set. Queries must
    // map it back to the CURRENT logical coordinates, including saved history.
    QImage qtImage(80, 70, QImage::Format_ARGB32);
    PkImage pkImage(80, 70, PkImage::Format_ARGB32);
    QPainter qtPainter(&qtImage);
    PkImageRasterBackend backend(pkImage);
    PkPainter painter(backend);
    int checkpoint = 0;
    const auto compare = [&] {
        ++checkpoint;
        const auto expected = qtPainter.clipBoundingRect();
        const auto actual = painter.clipBoundingRect();
        QCOMPARE(painter.hasClipping(), qtPainter.hasClipping());
        const QString context = QStringLiteral("checkpoint=%1 Qt=(%2,%3,%4,%5) Pk=(%6,%7,%8,%9)")
            .arg(checkpoint).arg(expected.x(), 0, 'g', 16).arg(expected.y(), 0, 'g', 16)
            .arg(expected.width(), 0, 'g', 16).arg(expected.height(), 0, 'g', 16)
            .arg(actual.x(), 0, 'g', 16).arg(actual.y(), 0, 'g', 16)
            .arg(actual.width(), 0, 'g', 16).arg(actual.height(), 0, 'g', 16);
        QVERIFY2(std::abs(actual.x() - expected.x()) < 1e-9, qPrintable(context));
        QVERIFY(std::abs(actual.y() - expected.y()) < 1e-9);
        QVERIFY(std::abs(actual.width() - expected.width()) < 1e-9);
        QVERIFY(std::abs(actual.height() - expected.height()) < 1e-9);
        QImage qtClip(80, 70, QImage::Format_ARGB32);
        PkImage pkClip(80, 70, PkImage::Format_ARGB32);
        qtClip.fill(0); pkClip.fill(0);
        QPainter qtClipPainter(&qtClip);
        PkImageRasterBackend clipBackend(pkClip);
        PkPainter clipPainter(clipBackend);
        qtClipPainter.setRenderHint(QPainter::Antialiasing);
        clipPainter.setRenderHint(PkPainter::Antialiasing);
        qtClipPainter.fillPath(qtPainter.clipPath(), QColor(30, 120, 220, 180));
        clipPainter.fillPath(painter.clipPath(), PkBrush(PkColor(30, 120, 220, 180)));
        qtClipPainter.end();
        for (int y = 0; y < 70; ++y) for (int x = 0; x < 80; ++x) {
            const auto pixelContext = context + QStringLiteral(" pixel=(%1,%2) Qt=%3 Pk=%4")
                .arg(x).arg(y).arg(qtClip.pixel(x,y), 8, 16, QLatin1Char('0'))
                .arg(pkClip.pixel(x,y), 8, 16, QLatin1Char('0'));
            QVERIFY2(pkClip.pixel(x,y) == qtClip.pixel(x,y), qPrintable(pixelContext));
        }
    };
    compare();
    qtPainter.translate(5, 3); painter.translate(5, 3);
    qtPainter.rotate(17); painter.rotate(17);
    qtPainter.setClipRect(QRectF(1.25, 2.5, 23, 19));
    painter.setClipRect(PkRectF(1.25, 2.5, 23, 19));
    compare();
    qtPainter.save(); painter.save();
    qtPainter.scale(0.75, 1.5); painter.scale(0.75, 1.5);
    compare();
    QPainterPath qtPath; PkPainterPath path;
    qtPath.moveTo(2, 3); path.moveTo(2, 3);
    qtPath.cubicTo(30, -2, 5, 23, 30, 20); path.cubicTo(30, -2, 5, 23, 30, 20);
    qtPath.lineTo(2, 24); path.lineTo(2, 24);
    qtPath.closeSubpath(); path.closeSubpath();
    qtPainter.setClipPath(qtPath, Qt::IntersectClip);
    painter.setClipPath(path, Pk::IntersectClip);
    compare();
    qtPainter.restore(); painter.restore();
    compare();
    qtPainter.setClipPath(qtPath); painter.setClipPath(path);
    qtPainter.rotate(-7); painter.rotate(-7);
    compare();
}

void PkImageRasterBackendTest::matchesQtTexturePathPixels()
{
    // Repeated image fill must follow the brush transform, not the outline's
    // bounding rectangle; bilinear taps must wrap across every tile seam.
    for (bool smooth : {false, true}) for (bool antialias : {false, true}) {
        QImage qtImage(39, 31, QImage::Format_ARGB32);
        PkImage pkImage(39, 31, PkImage::Format_ARGB32);
        qtImage.fill(0x80603040u); pkImage.fill(0x80603040u);
        QImage qtSource(7, 6, QImage::Format_ARGB32);
        PkImage pkSource(7, 6, PkImage::Format_ARGB32);
        std::mt19937 random(3827);
        for (int y = 0; y < 6; ++y) for (int x = 0; x < 7; ++x) {
            const uint32_t pixel = random();
            qtSource.setPixel(x, y, pixel); pkSource.setPixel(x, y, pixel);
        }
        QPainter qtPainter(&qtImage);
        PkImageRasterBackend backend(pkImage);
        PkPainter painter(backend);
        qtPainter.setRenderHint(QPainter::Antialiasing, antialias);
        painter.setRenderHint(PkPainter::Antialiasing, antialias);
        qtPainter.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
        painter.setRenderHint(PkPainter::SmoothPixmapTransform, smooth);
        qtPainter.translate(4, 2); painter.translate(4, 2);
        qtPainter.rotate(13); painter.rotate(13);
        qtPainter.setOpacity(0.9); painter.setOpacity(0.9);
        QPainterPath qtPath;
        PkPainterPath path;
        qtPath.moveTo(2, 3); path.moveTo(2, 3);
        qtPath.cubicTo(30, -2, 5, 23, 30, 20); path.cubicTo(30, -2, 5, 23, 30, 20);
        qtPath.lineTo(2, 24); path.lineTo(2, 24);
        qtPath.closeSubpath(); path.closeSubpath();
        QTransform qtTransform; PkTransform transform;
        qtTransform.translate(-2.25, 3.5); transform.translate(-2.25, 3.5);
        qtTransform.rotate(7); transform.rotate(7);
        qtTransform.scale(1.25, 0.8); transform.scale(1.25, 0.8);
        QBrush qtBrush(qtSource); qtBrush.setTransform(qtTransform);
        qtPainter.fillPath(qtPath, qtBrush);
        painter.fillTexturePath(path, pkSource, transform);
        qtPainter.end();
        for (int y = 0; y < 31; ++y) for (int x = 0; x < 39; ++x) {
            const QString context = QStringLiteral("smooth=%1 aa=%2 x=%3 y=%4 Qt=%5 Pk=%6")
                .arg(smooth).arg(antialias).arg(x).arg(y)
                .arg(qtImage.pixel(x, y), 8, 16, QLatin1Char('0'))
                .arg(pkImage.pixel(x, y), 8, 16, QLatin1Char('0'));
            QVERIFY2(pkImage.pixel(x, y) == qtImage.pixel(x, y), qPrintable(context));
        }
    }
}

void PkImageRasterBackendTest::matchesQtPatternImagePixels()
{
    // Catch incorrect source-rectangle mapping and tile phase/wrap at negative
    // offsets. Qt owns its pixmap and painter; no native expected-value helper.
    for (bool tiled : {false, true}) for (bool smooth : {false, true}) {
        for (bool antialias : {false, true}) {
            QImage qtImage(33, 27, QImage::Format_ARGB32);
            PkImage pkImage(33, 27, PkImage::Format_ARGB32);
            qtImage.fill(0x80603040u); pkImage.fill(0x80603040u);
            QImage qtSource(7, 6, QImage::Format_ARGB32_Premultiplied);
            PkImage pkSource(7, 6, PkImage::Format_ARGB32_Premultiplied);
            std::mt19937 random(3117);
            for (int y = 0; y < 6; ++y) for (int x = 0; x < 7; ++x) {
                const uint32_t pixel = qPremultiply(random());
                qtSource.setPixel(x, y, pixel); pkSource.setPixel(x, y, pixel);
            }
            QPainter qtPainter(&qtImage);
            PkImageRasterBackend backend(pkImage);
            PkPainter painter(backend);
            qtPainter.setRenderHint(QPainter::Antialiasing, antialias);
            painter.setRenderHint(PkPainter::Antialiasing, antialias);
            qtPainter.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
            painter.setRenderHint(PkPainter::SmoothPixmapTransform, smooth);
            qtPainter.translate(5, 2); painter.translate(5, 2);
            qtPainter.rotate(13); painter.rotate(13);
            qtPainter.setOpacity(0.9); painter.setOpacity(0.9);
            if (tiled) {
                qtPainter.drawTiledPixmap(QRectF(1.25, 2.5, 24, 19), QPixmap::fromImage(qtSource), QPointF(-2.25, 1.5));
                painter.drawTiledPixmap(PkRectF(1.25, 2.5, 24, 19), pkSource, PkPointF(-2.25, 1.5));
            } else {
                qtPainter.drawPixmap(QRectF(1.25, 2.5, 24, 19), QPixmap::fromImage(qtSource), QRectF(1.25, 0.5, 4.5, 4.25));
                painter.drawPixmap(PkRectF(1.25, 2.5, 24, 19), pkSource, PkRectF(1.25, 0.5, 4.5, 4.25));
            }
            qtPainter.end();
            for (int y = 0; y < 27; ++y) for (int x = 0; x < 33; ++x) {
                const QString context = QStringLiteral("tiled=%1 smooth=%2 aa=%3 x=%4 y=%5 Qt=%6 Pk=%7")
                    .arg(tiled).arg(smooth).arg(antialias).arg(x).arg(y)
                    .arg(qtImage.pixel(x, y), 8, 16, QLatin1Char('0'))
                    .arg(pkImage.pixel(x, y), 8, 16, QLatin1Char('0'));
                QVERIFY2(pkImage.pixel(x, y) == qtImage.pixel(x, y), qPrintable(context));
            }
        }
    }
}

void PkImageRasterBackendTest::matchesQtTransformedImagePixels()
{
    for (int format = 0; format < 4; ++format) {
    for (bool plus : {false, true}) {
    for (bool smooth : {false, true}) {
        for (bool antialias : {false, true}) {
            QImage qtImage(25, 22, QImage::Format_ARGB32);
            PkImage pkImage(25, 22, PkImage::Format_ARGB32);
            qtImage.fill(0x80603040u); pkImage.fill(0x80603040u);
            const QImage::Format qtFormats[] = {QImage::Format_ARGB32, QImage::Format_ARGB32_Premultiplied,
                QImage::Format_Grayscale8, QImage::Format_Mono};
            const PkImage::Format pkFormats[] = {PkImage::Format_ARGB32, PkImage::Format_ARGB32_Premultiplied,
                PkImage::Format_Grayscale8, PkImage::Format_Mono};
            QImage qtSource(5, 4, qtFormats[format]);
            PkImage pkSource(5, 4, pkFormats[format]);
            if (format == 3) {
                qtSource.setColorTable({0xff000000u, 0xffffffffu});
                pkSource.setColorTable({0xff000000u, 0xffffffffu});
            }
            std::mt19937 random(3217);
            for (int y = 0; y < 4; ++y) for (int x = 0; x < 5; ++x) {
                uint32_t pixel = random();
                if (format == 1) pixel = qPremultiply(pixel);
                if (format == 3) pixel = (x + y) % 2;
                if (format == 2) {
                    qtSource.scanLine(y)[x] = pixel & 255;
                    pkSource.scanLine(y)[x] = pixel & 255;
                } else {
                    qtSource.setPixel(x, y, pixel); pkSource.setPixel(x, y, pixel);
                }
            }
            QPainter qtPainter(&qtImage);
            PkImageRasterBackend backend(pkImage);
            PkPainter painter(backend);
            qtPainter.setRenderHint(QPainter::Antialiasing, antialias);
            painter.setRenderHint(PkPainter::Antialiasing, antialias);
            qtPainter.setRenderHint(QPainter::SmoothPixmapTransform, smooth);
            painter.setRenderHint(PkPainter::SmoothPixmapTransform, smooth);
            qtPainter.setClipRect(QRectF(2, 1, 17, 14));
            painter.setClipRect(PkRectF(2, 1, 17, 14));
            qtPainter.translate(5, 2); painter.translate(5, 2);
            qtPainter.rotate(17); painter.rotate(17);
            qtPainter.setOpacity(0.9); painter.setOpacity(0.9);
            if (plus) {
                qtPainter.setCompositionMode(QPainter::CompositionMode_Plus);
                painter.setCompositionMode(Pk::CompositionMode_Plus);
            }
            qtPainter.drawImage(QRectF(1.25, 2.5, 14, 9), qtSource);
            painter.drawImage(PkRectF(1.25, 2.5, 14, 9), pkSource);
            qtPainter.end();
            for (int y = 0; y < 22; ++y) for (int x = 0; x < 25; ++x) {
                const QString context = QStringLiteral("smooth=%1 aa=%2 x=%3 y=%4 Qt=%5 Pk=%6 format=%7 plus=%8")
                    .arg(smooth).arg(antialias).arg(x).arg(y)
                    .arg(qtImage.pixel(x, y), 8, 16, QLatin1Char('0'))
                    .arg(pkImage.pixel(x, y), 8, 16, QLatin1Char('0')).arg(format).arg(plus);
                QVERIFY2(pkImage.pixel(x, y) == qtImage.pixel(x, y), qPrintable(context));
            }
        }
    }
    }
    }
}

void PkImageRasterBackendTest::matchesQtGradientPixels()
{
    for (int kind = 0; kind < 3; ++kind) {
    for (int stopCount = 1; stopCount <= 3; ++stopCount) {
    for (double opacity : {0.25, 0.9, 1.0}) {
        for (int spread = 0; spread < 3; ++spread) {
            QImage qtImage(41, 23, QImage::Format_ARGB32);
            PkImage pkImage(41, 23, PkImage::Format_ARGB32);
            qtImage.fill(0x80602040u); pkImage.fill(0x80602040u);
            std::unique_ptr<QGradient> qtStorage;
            if (kind == 0) qtStorage = std::make_unique<QLinearGradient>(5, 3, 28, 17);
            else if (kind == 1) qtStorage = std::make_unique<QRadialGradient>(20, 12, 17, 16, 9);
            else qtStorage = std::make_unique<QConicalGradient>(20, 12, 37);
            QGradient &qtGradient = *qtStorage;
            auto pkGradient = PkGradient::linear(PkPointF(5, 3), PkPointF(28, 17));
            if (kind == 1) pkGradient = PkGradient::radial(PkPointF(20, 12), 17, PkPointF(16, 9));
            else if (kind == 2) pkGradient = PkGradient::conical(PkPointF(20, 12), 37);
            qtGradient.setColorAt(0, QColor(250, 15, 40, 71));
            pkGradient.setColorAt(0, PkColor(250, 15, 40, 71));
            if (stopCount >= 2) {
                qtGradient.setColorAt(1, QColor(20, 210, 185, 243));
                pkGradient.setColorAt(1, PkColor(20, 210, 185, 243));
            }
            if (stopCount == 3) {
                qtGradient.setColorAt(0.4, QColor(130, 75, 215, 160));
                pkGradient.setColorAt(0.4, PkColor(130, 75, 215, 160));
            }
            qtGradient.setSpread(static_cast<QGradient::Spread>(spread));
            pkGradient.setSpread(spread == 0 ? PkGradient::PadSpread :
                                 spread == 1 ? PkGradient::ReflectSpread : PkGradient::RepeatSpread);
            QBrush qtBrush(qtGradient);
            PkBrush pkBrush(pkGradient);
            qtBrush.setTransform(QTransform(1.25, 0.1, 0.2, 0.8, 1.5, -2));
            pkBrush.setTransform(PkTransform(1.25, 0.1, 0.2, 0.8, 1.5, -2));
            QPainter qtPainter(&qtImage);
            PkImageRasterBackend backend(pkImage);
            PkPainter painter(backend);
            qtPainter.setOpacity(opacity); painter.setOpacity(opacity);
            qtPainter.fillRect(QRectF(0, 0, 41, 23), qtBrush);
            painter.fillRect(PkRectF(0, 0, 41, 23), pkBrush);
            qtPainter.end();
            for (int y = 0; y < 23; ++y) for (int x = 0; x < 41; ++x) {
                const QString context = QStringLiteral("opacity=%1 spread=%2 x=%3 y=%4 Qt=%5 Pk=%6 kind=%7 stops=%8")
                    .arg(opacity).arg(spread).arg(x).arg(y)
                    .arg(qtImage.pixel(x, y), 8, 16, QLatin1Char('0'))
                    .arg(pkImage.pixel(x, y), 8, 16, QLatin1Char('0')).arg(kind).arg(stopCount);
                QVERIFY2(pkImage.pixel(x, y) == qtImage.pixel(x, y), qPrintable(context));
            }
        }
    }
    }
    }
}

void PkImageRasterBackendTest::matchesQtStrokePixels()
{
    for (bool dispatch : {false, true}) {
    for (double width : {0.0, 1.0, 3.25}) {
    for (bool antialias : {false, true}) {
        for (bool dashed : {false, true}) {
            QImage qtImage(40, 32, QImage::Format_ARGB32);
            PkImage pkImage(40, 32, PkImage::Format_ARGB32);
            qtImage.fill(0u); pkImage.fill(0u);
            QPainter qtPainter(&qtImage);
            PkImageRasterBackend backend(pkImage);
            PkPainter painter(backend);
            qtPainter.setRenderHint(QPainter::Antialiasing, antialias);
            painter.setRenderHint(PkPainter::Antialiasing, antialias);
            qtPainter.translate(3.5, 2.25); painter.translate(3.5, 2.25);
            QPainterPath qtPath;
            PkPainterPath pkPath;
            qtPath.moveTo(2, 3); pkPath.moveTo(2, 3);
            qtPath.lineTo(22, 5); pkPath.lineTo(22, 5);
            qtPath.lineTo(12, 24); pkPath.lineTo(12, 24);
            QPen qtPen(QColor(71, 133, 237, 198), width);
            PkPen pkPen(PkColor(71, 133, 237, 198), width);
            qtPen.setCapStyle(Qt::RoundCap); pkPen.setCapStyle(Pk::RoundCap);
            qtPen.setJoinStyle(Qt::MiterJoin); pkPen.setJoinStyle(Pk::MiterJoin);
            if (dashed) { qtPen.setStyle(Qt::DashLine); pkPen.setStyle(Pk::DashLine); }
            if (dispatch) {
                qtPainter.setPen(qtPen); painter.setPen(pkPen);
                qtPainter.drawPath(qtPath); painter.drawPath(pkPath);
                qtPainter.save(); painter.save();
                qtPainter.setPen(Qt::NoPen); painter.setPen(Pk::NoPen);
                qtPainter.drawLine(QPointF(1, 1), QPointF(10, 1));
                painter.drawLine(PkPointF(1, 1), PkPointF(10, 1));
                qtPainter.restore(); painter.restore();
                qtPainter.drawLine(QPointF(1, 1), QPointF(10, 1));
                painter.drawLine(PkPointF(1, 1), PkPointF(10, 1));
                if (width == 0) {
                    qtPainter.drawPoint(QPointF(29.5, 5.25));
                    painter.drawPoint(PkPointF(29.5, 5.25));
                }
            } else {
                qtPainter.strokePath(qtPath, qtPen);
                painter.strokePath(pkPath, pkPen);
            }
            qtPainter.end();
            for (int y = 0; y < 32; ++y) {
                for (int x = 0; x < 40; ++x) {
                    const QString context = QStringLiteral("aa=%1 dash=%2 x=%3 y=%4 Qt=%5 Pk=%6")
                        .arg(antialias).arg(dashed).arg(x).arg(y)
                        .arg(qtImage.pixel(x, y), 8, 16, QLatin1Char('0'))
                        .arg(pkImage.pixel(x, y), 8, 16, QLatin1Char('0'));
                    QVERIFY2(pkImage.pixel(x, y) == qtImage.pixel(x, y), qPrintable(context));
                }
            }
        }
    }
    }
    }
}

void PkImageRasterBackendTest::matchesQtTransformedClippedPathCoverage()
{
    // Exact area coverage, cubic subdivision, winding holes, device-space
    // clip persistence and Source clearing are independently rendered by Qt.
    for (bool antialias : {false, true}) {
    for (double opacity : {0.0, 0.25, 0.4, 0.9, 1.0}) {
    for (bool oddEven : {false, true}) {
        QImage qtImage(32, 24, QImage::Format_ARGB32);
        PkImage pkImage(32, 24, PkImage::Format_ARGB32);
        qtImage.fill(0x80604020u);
        pkImage.fill(0x80604020u);
        QPainter qtPainter(&qtImage);
        PkImageRasterBackend backend(pkImage);
        PkPainter painter(backend);
        qtPainter.setRenderHint(QPainter::Antialiasing, antialias);
        painter.setRenderHint(PkPainter::Antialiasing, antialias);
        qtPainter.translate(2.25, 1.5);
        painter.translate(2.25, 1.5);
        qtPainter.setClipRect(QRectF(1, 2, 23, 17));
        painter.setClipRect(PkRectF(1, 2, 23, 17));
        qtPainter.save();
        painter.save();
        qtPainter.rotate(13);
        painter.rotate(13);
        qtPainter.setOpacity(opacity);
        painter.setOpacity(opacity);
        QPainterPath qtPath;
        PkPainterPath pkPath;
        qtPath.moveTo(2, 2); pkPath.moveTo(2, 2);
        qtPath.cubicTo(30, -2, -2, 28, 25, 18);
        pkPath.cubicTo(30, -2, -2, 28, 25, 18);
        qtPath.lineTo(3, 20); pkPath.lineTo(3, 20);
        qtPath.closeSubpath(); pkPath.closeSubpath();
        qtPath.addRect(QRectF(5, 6, 8, 9));
        pkPath.addRect(PkRectF(5, 6, 8, 9));
        qtPath.setFillRule(oddEven ? Qt::OddEvenFill : Qt::WindingFill);
        pkPath.setFillRule(oddEven ? Pk::OddEvenFill : Pk::WindingFill);
        qtPainter.fillPath(qtPath, QColor(123, 231, 87, 179));
        painter.fillPath(pkPath, PkBrush(PkColor(123, 231, 87, 179)));
        qtPainter.restore();
        painter.restore();
        qtPainter.setCompositionMode(QPainter::CompositionMode_Source);
        painter.setCompositionMode(Pk::CompositionMode_Source);
        qtPainter.fillRect(QRectF(4, 4, 5, 6), Qt::transparent);
        painter.fillRect(PkRectF(4, 4, 5, 6), PkBrush(PkColor(Pk::transparent)));
        qtPainter.end();
        for (int y = 0; y < 24; ++y) {
            for (int x = 0; x < 32; ++x) {
                const QString context = QStringLiteral("oddEven=%1 x=%2 y=%3 Qt=%4 Pk=%5")
                    .arg(oddEven).arg(x).arg(y)
                    .arg(qtImage.pixel(x, y), 8, 16, QLatin1Char('0'))
                    .arg(pkImage.pixel(x, y), 8, 16, QLatin1Char('0'));
                QVERIFY2(pkImage.pixel(x, y) == qtImage.pixel(x, y), qPrintable(context));
            }
        }
    }
    }
    }
}

void PkImageRasterBackendTest::matchesQtPlusPixelsAndOverlappingMasks()
{
    // A fixed SourceOver backend, pre-saturation opacity, or lost saved
    // composition state all change these pixels. Qt supplies the reference.
    for (int width : {1, 2, 3, 4, 7, 8, 9, 15, 17, 65}) {
        QImage qtSource(width, 24, QImage::Format_ARGB32);
        QImage qtOriginal(width, 24, QImage::Format_ARGB32);
        PkImage pkSource(width, 24, PkImage::Format_ARGB32);
        PkImage pkOriginal(width, 24, PkImage::Format_ARGB32);
        std::mt19937 random(1847);
        constexpr unsigned edges[] = {0, 1, 63, 127, 128, 254, 255};
        for (int y = 0; y < 24; ++y) {
            for (int x = 0; x < width; ++x) {
                // The first rows are white glyph coverage masks, including
                // transparent, low alpha, and saturated overlap boundaries.
                const uint32_t source = y < 7 ? (edges[y] << 24) | 0xffffffu : random();
                const uint32_t destination = y < 7 ?
                    (edges[(x + y) % 7] << 24) | 0xffffffu : random();
                qtSource.setPixel(x, y, source);
                pkSource.setPixel(x, y, source);
                qtOriginal.setPixel(x, y, destination);
                pkOriginal.setPixel(x, y, destination);
            }
        }
        for (double opacity : {0.0, 1.0 / 256, 0.1, 0.25, 0.5, 0.9, 255.0 / 256, 1.0}) {
            QImage qtDestination = qtOriginal;
            PkImage pkDestination = pkOriginal;
            QPainter qtPainter(&qtDestination);
            PkImageRasterBackend backend(pkDestination);
            PkPainter painter(backend);
            qtPainter.setOpacity(opacity);
            painter.setOpacity(opacity);
            qtPainter.setCompositionMode(QPainter::CompositionMode_Plus);
            painter.setCompositionMode(Pk::CompositionMode_Plus);
            for (int pass = 0; pass < 3; ++pass) {
                const int offset = pass - 1;
                qtPainter.drawImage(QPoint(offset, 0), qtSource);
                painter.drawImage(PkRectF(offset, 0, width, 24), pkSource);
                for (int y = 0; y < 24; ++y) {
                    for (int x = 0; x < width; ++x) {
                        const QString context = QStringLiteral("width=%1 opacity=%2 pass=%3 x=%4 y=%5 Qt=%6 Pk=%7")
                            .arg(width).arg(opacity, 0, 'g', 17).arg(pass).arg(x).arg(y)
                            .arg(qtDestination.pixel(x, y), 8, 16, QLatin1Char('0'))
                            .arg(pkDestination.pixel(x, y), 8, 16, QLatin1Char('0'));
                        QVERIFY2(pkDestination.pixel(x, y) == qtDestination.pixel(x, y), qPrintable(context));
                    }
                }
            }
            qtPainter.save();
            painter.save();
            qtPainter.setCompositionMode(QPainter::CompositionMode_SourceOver);
            painter.setCompositionMode(Pk::CompositionMode_SourceOver);
            qtPainter.setOpacity(0.25);
            painter.setOpacity(0.25);
            qtPainter.drawImage(QPoint(), qtSource);
            painter.drawImage(PkRectF(0, 0, width, 24), pkSource);
            qtPainter.restore();
            painter.restore();
            QCOMPARE(painter.compositionMode(), Pk::CompositionMode_Plus);
            QCOMPARE(painter.opacity(), opacity);
            qtPainter.drawImage(QPoint(), qtSource);
            painter.drawImage(PkRectF(0, 0, width, 24), pkSource);
            qtPainter.end();
            for (int y = 0; y < 24; ++y) {
                for (int x = 0; x < width; ++x) {
                    QCOMPARE(pkDestination.pixel(x, y), qtDestination.pixel(x, y));
                }
            }
        }
    }
}

void PkImageRasterBackendTest::reportsDestinationDevicePixelRatio()
{
    PkImage destination(4, 4, PkImage::Format_ARGB32);
    destination.setDevicePixelRatio(1.75);
    PkImageRasterBackend backend(destination);
    PkPainter painter(backend);

    QCOMPARE(painter.devicePixelRatio(), 1.75);
}

void PkImageRasterBackendTest::blendsImageWithOpacity()
{
    PkImage destination(3, 1, PkImage::Format_ARGB32);
    destination.setPixel(0, 0, 0xff202020u);
    destination.setPixel(1, 0, 0x804080c0u);
    destination.setPixel(2, 0, 0xd4616161u);

    PkImage opaqueSource(1, 1, PkImage::Format_ARGB32);
    opaqueSource.setPixel(0, 0, 0xffe06020u);
    PkImage translucentSource(1, 1, PkImage::Format_ARGB32);
    translucentSource.setPixel(0, 0, 0x80e02060u);
    PkImage reviewerSource(1, 1, PkImage::Format_ARGB32);
    reviewerSource.setPixel(0, 0, 0xd49c22acu);

    PkImageRasterBackend backend(destination);
    PkPainter painter(backend);
    painter.setOpacity(0.25);
    painter.drawImage(PkRectF(0, 0, 1, 1), opaqueSource);
    painter.setOpacity(0.5);
    painter.drawImage(PkRectF(1, 0, 1, 1), translucentSource);
    painter.setOpacity(0.1);
    painter.drawImage(PkRectF(2, 0, 1, 1), reviewerSource);

    QCOMPARE(destination.pixel(0, 0), 0xff4f3020u);
    QCOMPARE(destination.pixel(1, 0), 0xa0805a9au);
    QCOMPARE(destination.pixel(2, 0), 0xd7665b68u);
}

void PkImageRasterBackendTest::matchesQtArgb32SourceOverMatrix()
{
    constexpr std::array<unsigned, 12> boundaries {
        0, 1, 2, 15, 16, 63, 64, 127, 128, 129, 254, 255
    };
    constexpr std::array<qreal, 18> opacities {
        0.0, 0.1, 1.0 / 256.0, 1.0 / 255.0, 0.01, 0.25,
        63.0 / 256.0, 64.0 / 256.0, 127.0 / 256.0,
        0.5, 128.0 / 255.0, 129.0 / 256.0, 0.75, 0.9,
        254.0 / 256.0, 254.0 / 255.0, 255.0 / 256.0, 1.0
    };

    std::vector<uint32_t> sources;
    std::vector<uint32_t> destinations;
    for (unsigned a : boundaries) {
        for (unsigned value : boundaries) {
            sources.push_back(a == 0 ? 0 :
                              (a << 24) |
                                  (value << 16) |
                                  (((value * 73u + 19u) & 0xffu) << 8) |
                                  ((value * 151u + 7u) & 0xffu));
            destinations.push_back(a == 0 ? 0 :
                                   (a << 24) | (value << 16) |
                                       (value << 8) | value);
        }
    }

    const int pixelCount = static_cast<int>(sources.size() * destinations.size());
    for (qreal opacity : opacities) {
        QImage qtSource(pixelCount, 1, QImage::Format_ARGB32);
        QImage qtDestination(pixelCount, 1, QImage::Format_ARGB32);
        PkImage pkSource(pixelCount, 1, PkImage::Format_ARGB32);
        PkImage pkDestination(pixelCount, 1, PkImage::Format_ARGB32);

        int x = 0;
        for (uint32_t source : sources) {
            for (uint32_t destination : destinations) {
                qtSource.setPixel(x, 0, source);
                qtDestination.setPixel(x, 0, destination);
                pkSource.setPixel(x, 0, source);
                pkDestination.setPixel(x, 0, destination);
                ++x;
            }
        }

        QPainter qtPainter(&qtDestination);
        qtPainter.setOpacity(opacity);
        qtPainter.drawImage(QPoint(), qtSource);
        qtPainter.end();

        PkImageRasterBackend backend(pkDestination);
        PkPainter painter(backend);
        painter.setOpacity(opacity);
        painter.drawImage(PkRectF(0, 0, pixelCount, 1), pkSource);

        int mismatches = 0;
        int firstMismatch = -1;
        QString mismatchDetails;
        for (int i = 0; i < pixelCount; ++i) {
            if (qtDestination.pixel(i, 0) != pkDestination.pixel(i, 0)) {
                if (firstMismatch < 0) {
                    firstMismatch = i;
                }
                if (mismatches < 8) {
                    const auto source = sources.at(
                        static_cast<std::size_t>(i) / destinations.size());
                    const auto destination = destinations.at(
                        static_cast<std::size_t>(i) % destinations.size());
                    mismatchDetails += QStringLiteral(
                        " [%1 src=%2 dst=%3 Qt=%4 Pk=%5]")
                        .arg(i)
                        .arg(source, 8, 16, QLatin1Char('0'))
                        .arg(destination, 8, 16, QLatin1Char('0'))
                        .arg(qtDestination.pixel(i, 0), 8, 16, QLatin1Char('0'))
                        .arg(pkDestination.pixel(i, 0), 8, 16, QLatin1Char('0'));
                }
                ++mismatches;
            }
        }

        const QString diagnostic = QStringLiteral(
            "opacity=%1 mismatches=%2/%3 first=%4 Qt=%5 Pk=%6")
            .arg(opacity, 0, 'g', 17)
            .arg(mismatches)
            .arg(pixelCount)
            .arg(firstMismatch)
            .arg(firstMismatch >= 0 ? qtDestination.pixel(firstMismatch, 0) : 0,
                 8, 16, QLatin1Char('0'))
            .arg(firstMismatch >= 0 ? pkDestination.pixel(firstMismatch, 0) : 0,
                 8, 16, QLatin1Char('0')) + mismatchDetails;
        QVERIFY2(mismatches == 0, qPrintable(diagnostic));
    }
}

void PkImageRasterBackendTest::matchesQtShortSpansAndTails()
{
    constexpr std::array<qreal, 10> opacities {
        0.0, 1.0 / 256.0, 0.1, 0.25, 127.0 / 256.0,
        0.5, 0.75, 0.9, 255.0 / 256.0, 1.0
    };

    // Exact minimum reproduction from the production-closure review. The
    // transparent neighbor is significant because Qt fetches ARGB32 pixels
    // in groups before composing and storing the short span.
    {
        QImage qtSource(2, 1, QImage::Format_ARGB32);
        QImage qtDestination(2, 1, QImage::Format_ARGB32);
        PkImage pkSource(2, 1, PkImage::Format_ARGB32);
        PkImage pkDestination(2, 1, PkImage::Format_ARGB32);
        constexpr std::array<uint32_t, 2> sources {0x00ebcc83u, 0x3caf8806u};
        constexpr std::array<uint32_t, 2> destinations {0x00cbcbcbu, 0x3c818181u};
        for (int x = 0; x < 2; ++x) {
            qtSource.setPixel(x, 0, sources[static_cast<std::size_t>(x)]);
            qtDestination.setPixel(x, 0, destinations[static_cast<std::size_t>(x)]);
            pkSource.setPixel(x, 0, sources[static_cast<std::size_t>(x)]);
            pkDestination.setPixel(x, 0, destinations[static_cast<std::size_t>(x)]);
        }

        QPainter qtPainter(&qtDestination);
        qtPainter.setOpacity(0.9);
        qtPainter.drawImage(QPoint(), qtSource);
        qtPainter.end();

        PkImageRasterBackend backend(pkDestination);
        PkPainter painter(backend);
        painter.setOpacity(0.9);
        painter.drawImage(PkRectF(0, 0, 2, 1), pkSource);

        QCOMPARE(qtDestination.pixel(1, 0), 0x65998540u);
        QCOMPARE(pkDestination.pixel(1, 0), qtDestination.pixel(1, 0));
    }

    constexpr int seedCount = 200;
    for (int width = 1; width <= 65; ++width) {
        QImage qtSource(width, seedCount, QImage::Format_ARGB32);
        QImage originalQtDestination(width, seedCount, QImage::Format_ARGB32);
        PkImage pkSource(width, seedCount, PkImage::Format_ARGB32);
        PkImage originalPkDestination(width, seedCount, PkImage::Format_ARGB32);

        for (int seed = 0; seed < seedCount; ++seed) {
            std::mt19937 random(static_cast<std::mt19937::result_type>(seed));
            for (int x = 0; x < width; ++x) {
                const uint32_t source = random();
                const unsigned destinationAlpha = random() >> 24;
                const unsigned gray = random() & 0xffu;
                const uint32_t destination = (destinationAlpha << 24) |
                    (gray << 16) | (gray << 8) | gray;
                qtSource.setPixel(x, seed, source);
                originalQtDestination.setPixel(x, seed, destination);
                pkSource.setPixel(x, seed, source);
                originalPkDestination.setPixel(x, seed, destination);
            }
        }

        for (qreal opacity : opacities) {
            QImage qtDestination = originalQtDestination;
            PkImage pkDestination = originalPkDestination;

            QPainter qtPainter(&qtDestination);
            qtPainter.setOpacity(opacity);
            qtPainter.drawImage(QPoint(), qtSource);
            qtPainter.end();

            PkImageRasterBackend backend(pkDestination);
            PkPainter painter(backend);
            painter.setOpacity(opacity);
            painter.drawImage(PkRectF(0, 0, width, seedCount), pkSource);

            for (int seed = 0; seed < seedCount; ++seed) {
                for (int x = 0; x < width; ++x) {
                    const uint32_t qtPixel = qtDestination.pixel(x, seed);
                    const uint32_t pkPixel = pkDestination.pixel(x, seed);
                    const QString diagnostic = QStringLiteral(
                        "width=%1 tail=%2 seed=%3 x=%4 opacity=%5 "
                        "src=%6 dst=%7 Qt=%8 Pk=%9")
                        .arg(width)
                        .arg(width % 8)
                        .arg(seed)
                        .arg(x)
                        .arg(opacity, 0, 'g', 17)
                        .arg(qtSource.pixel(x, seed), 8, 16, QLatin1Char('0'))
                        .arg(originalQtDestination.pixel(x, seed), 8, 16,
                             QLatin1Char('0'))
                        .arg(qtPixel, 8, 16, QLatin1Char('0'))
                        .arg(pkPixel, 8, 16, QLatin1Char('0'));
                    QVERIFY2(pkPixel == qtPixel, qPrintable(diagnostic));
                }
            }
        }
    }
}

void PkImageRasterBackendTest::clipsToDestinationBounds()
{
    PkImage destination(1, 1, PkImage::Format_ARGB32);
    destination.fill(0xff000000u);

    PkImage source(2, 1, PkImage::Format_ARGB32);
    source.setPixel(0, 0, 0xffff0000u);
    source.setPixel(1, 0, 0xff00ff00u);

    PkImageRasterBackend backend(destination);
    PkPainter painter(backend);
    painter.drawImage(PkRectF(-1, 0, 2, 1), source);

    QCOMPARE(destination.pixel(0, 0), 0xff00ff00u);
}

void PkImageRasterBackendTest::rejectsUnsupportedOperations()
{
    PkImage destination(1, 1, PkImage::Format_ARGB32);
    PkImage source(1, 1, PkImage::Format_ARGB32);
    PkImageRasterBackend backend(destination);
    PkPainter painter(backend);

    painter.setPen(PkPen(Pk::NoPen));
    painter.setBrush(PkBrush(PkColor(Pk::red)));
    painter.drawRect(PkRectF(0, 0, 1, 1));
    QCOMPARE(destination.pixel(0, 0), 0xffff0000u);
    QVERIFY_EXCEPTION_THROWN(painter.drawEllipse(PkRectF(0, 0, 1, 1)), std::logic_error);

    PkImage unsupportedDestination(1, 1, PkImage::Format_RGBA8888);
    PkImageRasterBackend unsupportedBackend(unsupportedDestination);
    PkPainter unsupportedPainter(unsupportedBackend);
    QVERIFY_EXCEPTION_THROWN(
        unsupportedPainter.drawImage(PkRectF(0, 0, 1, 1), source),
        std::invalid_argument);
}

QTEST_MAIN(PkImageRasterBackendTest)

#include "PkImageRasterBackendTest.moc"
