// R-64 聚焦探针：对单个用例，把 Pk(default ctor) 与 Qt(默认 generator) 两份文档
// 渲染成 32×32 后逐像素对拍，并把两边的隐式 viewBox 以全精度打印。
//
// 落点：pk/render/oracle/probes/focus.cpp（Qt 侧）。由同目录 run_probes.sh 编译运行；它回答什么、期望读数见同目录 README.md。
#include "svg_primitive_cases.h"

#include <QBuffer>
#include <QBrush>
#include <QByteArray>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QRect>
#include <QSize>
#include <QSvgGenerator>
#include <QSvgRenderer>

#include <fstream>
#include <iostream>
#include <map>
#include <string>

static const int kCanvas = 32;

static QByteArray renderQtDoc(const pkSvgCases::Case &c)
{
    QByteArray out; QBuffer buffer(&out); buffer.open(QIODevice::WriteOnly);
    QSvgGenerator generator; generator.setOutputDevice(&buffer); generator.setResolution(72);
    QPainter painter(&generator);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(c.hasPen ? QPen(Qt::black, c.penWidth) : QPen(Qt::NoPen));
    painter.setBrush(c.hasBrush ? QBrush(Qt::red) : QBrush(Qt::NoBrush));
    switch (c.kind) {
    case pkSvgCases::Kind::Ellipse: painter.drawEllipse(c.rect); break;
    case pkSvgCases::Kind::Polygon: { QPolygonF p; for (const auto &q : c.points) p << q; painter.drawPolygon(p); break; }
    case pkSvgCases::Kind::Arc: painter.drawArc(c.rect, c.startAngle16, c.spanAngle16); break;
    }
    painter.end();
    return out;
}

static QString g17(double v) { return QString::number(v, 'g', 17); }

int main(int argc, char **argv)
{
    if (argc < 3) { std::cerr << "usage: focus <pkdocs.txt> <case-name>\n"; return 2; }
    std::map<std::string, std::map<std::string, std::string>> docs;
    { std::ifstream in(argv[1]); std::string line;
      while (std::getline(in, line)) { auto t1 = line.find('\t'); auto t2 = line.find('\t', t1+1);
        if (t1 == std::string::npos || t2 == std::string::npos) continue;
        docs[line.substr(0,t1)][line.substr(t1+1, t2-t1-1)] = line.substr(t2+1); } }

    const std::string want = argv[2];
    for (const auto &c : pkSvgCases::table()) {
        if (c.name != want) continue;
        const QByteArray qtDoc = renderQtDoc(c);
        const QByteArray pkDoc = QByteArray::fromStdString(docs[c.name].at("default"));
        std::cout << "Qt doc : " << qtDoc.constData() << "\n";
        std::cout << "Pk doc : " << pkDoc.constData() << "\n";

        QSvgRenderer qr(qtDoc), pr(pkDoc);
        QRectF qvb = qr.viewBoxF(), pvb = pr.viewBoxF();
        std::cout << "qt viewBox: x=" << g17(qvb.x()).toStdString() << " y=" << g17(qvb.y()).toStdString()
                  << " w=" << g17(qvb.width()).toStdString() << " h=" << g17(qvb.height()).toStdString()
                  << "  aspectRatio=" << qr.aspectRatioMode() << "\n";
        std::cout << "pk viewBox: x=" << g17(pvb.x()).toStdString() << " y=" << g17(pvb.y()).toStdString()
                  << " w=" << g17(pvb.width()).toStdString() << " h=" << g17(pvb.height()).toStdString()
                  << "  aspectRatio=" << pr.aspectRatioMode() << "\n";
        std::cout << "delta viewBox: dx=" << g17(pvb.x()-qvb.x()).toStdString()
                  << " dy=" << g17(pvb.y()-qvb.y()).toStdString()
                  << " dw=" << g17(pvb.width()-qvb.width()).toStdString()
                  << " dh=" << g17(pvb.height()-qvb.height()).toStdString() << "\n";
        std::cout << "scale: qt=" << g17(32.0/qvb.width()).toStdString() << "x" << g17(32.0/qvb.height()).toStdString()
                  << "  pk=" << g17(32.0/pvb.width()).toStdString() << "x" << g17(32.0/pvb.height()).toStdString() << "\n";

        QImage qi(kCanvas, kCanvas, QImage::Format_ARGB32), pi(kCanvas, kCanvas, QImage::Format_ARGB32);
        qi.fill(0); pi.fill(0);
        { QPainter a(&qi); qr.render(&a); QPainter b(&pi); pr.render(&b); }
        int n = 0;
        for (int y = 0; y < kCanvas; ++y) {
            std::string qrow, prow, mrow;
            for (int x = 0; x < kCanvas; ++x) {
                const QRgb a = qi.pixel(x, y), b = pi.pixel(x, y);
                qrow += (qAlpha(a) ? '#' : '.');
                prow += (qAlpha(b) ? '#' : '.');
                if (a != b) { mrow += 'X'; ++n;
                    std::cout << "  px(" << x << "," << y << ") qt=" << std::hex << a << " pk=" << b << std::dec
                              << " dA=" << (qAlpha(b)-qAlpha(a)) << "\n"; }
                else mrow += (qAlpha(a) ? '#' : '.');
            }
            std::cout << "  qt " << qrow << "\n  pk " << prow << "\n  mm " << mrow << "\n";
        }
        std::cout << "diff pixels = " << n << "\n";
    }
    return 0;
}
