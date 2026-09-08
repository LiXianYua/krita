/* SPDX-License-Identifier: GPL-2.0-or-later */
#include <QTest>
#include <QImage>
#include <QImageReader>
#include <QColorSpace>
#include <QTemporaryDir>
#include <png.h>
#include <cstdio>
#include "../../shapes/PkReferenceImageLoader.h"

class PkReferenceImageLoaderTest : public QObject {
    Q_OBJECT
private Q_SLOTS:
    void normalizesTaggedRasterPixels();
    void preservesHighDepthTaggedPng();
    void normalizesGammaChromaticityPng();
};
void PkReferenceImageLoaderTest::normalizesGammaChromaticityPng()
{
    QTemporaryDir dir; QVERIFY(dir.isValid());
    for (double gamma : {1.0,0.5,0.45455}) for (int primaries : {0,1,2,3,4}) for (bool highDepth : {false,true}) {
        const QString path=dir.filePath("gamma.png");
        FILE *file=std::fopen(path.toLocal8Bit().constData(),"wb"); QVERIFY(file);
        png_structp png=png_create_write_struct(PNG_LIBPNG_VER_STRING,nullptr,nullptr,nullptr); QVERIFY(png);
        png_infop info=png_create_info_struct(png); QVERIFY(info);
        QVERIFY(setjmp(png_jmpbuf(png))==0);
        png_init_io(png,file);
        png_set_IHDR(png,info,17,13,highDepth?16:8,PNG_COLOR_TYPE_RGBA,PNG_INTERLACE_NONE,PNG_COMPRESSION_TYPE_DEFAULT,PNG_FILTER_TYPE_DEFAULT);
        png_set_gAMA(png,info,gamma);
        if (primaries) png_set_cHRM(png,info,primaries==4?.34567:.31271,primaries==4?.35850:.32902,
            primaries==2?.68:.64,primaries==2?.32:.33,primaries==2?.265:.30,primaries==2?.69:.60,.15,.06);
        if (primaries==3) png_set_sRGB_gAMA_and_cHRM(png,info,PNG_sRGB_INTENT_PERCEPTUAL);
        png_write_info(png,info);
        for (int y=0;y<13;++y) {
            unsigned char row[17*8];
            for (int x=0;x<17;++x) {
                const unsigned values[] = {unsigned(x*19+y*31),unsigned(x*59+y*7),unsigned(x*11+y*43),unsigned(x*37+y*17)};
                for (int c=0;c<4;++c) {
                    if (highDepth) {
                        const unsigned value=(values[c]*251+c*137)%65536;
                        row[8*x+2*c]=value>>8; row[8*x+2*c+1]=value&255;
                    } else row[4*x+c]=values[c]%256;
                }
            }
            png_write_row(png,row);
        }
        png_write_end(png,info); png_destroy_write_struct(&png,&info); std::fclose(file);
        QImage expected=QImageReader(path).read();
        QVERIFY(expected.colorSpace().isValid());
        expected.convertToColorSpace(QColorSpace(QColorSpace::SRgb));
        const PkImage actual=loadPkReferenceImage(path.toStdString());
        QCOMPARE(actual.width(),expected.width());
        QCOMPARE(actual.depth(),expected.depth());
        if (highDepth) {
            for (int y=0;y<13;++y) {
                const auto *a=reinterpret_cast<const quint16*>(actual.constScanLine(y));
                const auto *e=reinterpret_cast<const quint16*>(expected.constScanLine(y));
                for (int i=0;i<17*4;++i) QVERIFY2(a[i]==e[i],qPrintable(QString("gamma=%1 primaries=%2 y=%3 channel=%4 Qt=%5 native=%6")
                    .arg(gamma).arg(primaries).arg(y).arg(i).arg(e[i]).arg(a[i])));
            }
            // Compare retained 16-bit storage above, not the separate image
            // classes' lossy pixel() conversion to eight-bit channels.
            continue;
        }
        for (int y=0;y<13;++y) for (int x=0;x<17;++x)
            QVERIFY2(actual.pixel(x,y)==expected.pixel(x,y),qPrintable(QString("gamma=%1 primaries=%2 x=%3 y=%4 Qt=%5 native=%6")
                .arg(gamma).arg(primaries).arg(x).arg(y).arg(expected.pixel(x,y),8,16,QLatin1Char('0')).arg(actual.pixel(x,y),8,16,QLatin1Char('0'))));
    }
}
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
