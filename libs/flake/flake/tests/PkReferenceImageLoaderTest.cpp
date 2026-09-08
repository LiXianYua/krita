/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <QTest>
#include <QImage>
#include <QImageReader>
#include <QColorSpace>
#include <QTemporaryDir>
#include "../../shapes/PkReferenceImageLoader.h"

class PkReferenceImageLoaderTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void normalizesTaggedRasterPixels();
    void preservesHighDepthTaggedPng();
};
void PkReferenceImageLoaderTest::normalizesTaggedRasterPixels()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    for (auto format : {"png", "jpg", "tiff"})
    for (auto space : {QColorSpace::SRgb, QColorSpace::SRgbLinear, QColorSpace::DisplayP3, QColorSpace::AdobeRgb, QColorSpace::ProPhotoRgb}) {
        QImage input(17, 13, QImage::Format_ARGB32);
        input.setColorSpace(QColorSpace(space));
        for (int y = 0; y < input.height(); ++y) for (int x = 0; x < input.width(); ++x)
            input.setPixel(x, y, qRgba((x*19+y*31)%256, (x*59+y*7)%256, (x*11+y*43)%256, (x*37+y*17)%256));
        const QString path = dir.filePath(QString::number(int(space)) + "." + format);
        QVERIFY(input.save(path));
        QImage expected = QImageReader(path).read();
        if (expected.colorSpace().isValid()) expected.convertToColorSpace(QColorSpace(QColorSpace::SRgb));
        const PkImage actual = loadPkReferenceImage(path.toStdString());
        QCOMPARE(actual.width(), expected.width());
        QCOMPARE(actual.height(), expected.height());
        for (int y = 0; y < input.height(); ++y) for (int x = 0; x < input.width(); ++x) {
            QVERIFY2(actual.pixel(x,y) == expected.pixel(x,y),
                qPrintable(QString("space=%1 x=%2 y=%3 Qt=%4 native=%5 format=%6").arg(space).arg(x).arg(y)
                    .arg(expected.pixel(x,y), 8, 16, QLatin1Char('0')).arg(actual.pixel(x,y), 8, 16, QLatin1Char('0')).arg(format)));
        }
    }
}
void PkReferenceImageLoaderTest::preservesHighDepthTaggedPng()
{
    QTemporaryDir dir;
    QVERIFY(dir.isValid());
    for (auto space : {QColorSpace::SRgbLinear,QColorSpace::DisplayP3}) {
        QImage input(7,5,QImage::Format_RGBA64);
        input.setColorSpace(QColorSpace(space));
        for (int y=0;y<5;++y) for (int x=0;x<7;++x)
            input.setPixelColor(x,y,QColor::fromRgba64((x*6111+y*1471)%65536,(x*1237+y*7979)%65536,
                (x*4793+y*4093)%65536,3000+x*6001+y*1009));
        const QString path=dir.filePath("high-depth.png");
        QVERIFY(input.save(path));
        QImage expected=QImageReader(path).read();
        expected.convertToColorSpace(QColorSpace(QColorSpace::SRgb));
        const PkImage actual=loadPkReferenceImage(path.toStdString());
        QCOMPARE(actual.depth(),expected.depth());
        for (int y=0;y<5;++y) {
            const auto *a=reinterpret_cast<const quint16*>(actual.constScanLine(y));
            const auto *e=reinterpret_cast<const quint16*>(expected.constScanLine(y));
            for (int i=0;i<7*4;++i) QCOMPARE(a[i],e[i]);
        }
    }
}
QTEST_MAIN(PkReferenceImageLoaderTest)
#include "PkReferenceImageLoaderTest.moc"
