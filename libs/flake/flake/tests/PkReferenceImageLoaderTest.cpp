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
QTEST_MAIN(PkReferenceImageLoaderTest)
#include "PkReferenceImageLoaderTest.moc"
