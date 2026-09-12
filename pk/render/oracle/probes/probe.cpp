// R-64 探针：真 Qt 侧比较器。读 pkdocs 输出（每行 <name>\t<bounds|default>\t<doc>），
// 对每个用例把 Pk 两种文档与 Qt 参照文档都渲染成 32×32 ARGB32 再逐像素比。
//
// 落点：pk/render/oracle/probes/probe.cpp（Qt 侧）。由同目录 run_probes.sh 编译运行；它回答什么、期望读数见同目录 README.md。
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

#include <cstdint>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {
constexpr int kCanvas = 32;

QByteArray renderQtDoc(const pkSvgCases::Case &c, bool withViewBox)
{
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    QSvgGenerator generator;
    generator.setOutputDevice(&buffer);
    if (withViewBox) {
        generator.setSize(QSize(kCanvas, kCanvas));
        generator.setViewBox(QRect(0, 0, kCanvas, kCanvas));
    }
    generator.setResolution(72);
    QPainter painter(&generator);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(c.hasPen ? QPen(Qt::black, c.penWidth) : QPen(Qt::NoPen));
    painter.setBrush(c.hasBrush ? QBrush(Qt::red) : QBrush(Qt::NoBrush));
    switch (c.kind) {
    case pkSvgCases::Kind::Ellipse: painter.drawEllipse(c.rect); break;
    case pkSvgCases::Kind::Polygon: {
        QPolygonF polygon; for (const auto &p : c.points) polygon << p;
        painter.drawPolygon(polygon); break; }
    case pkSvgCases::Kind::Arc: painter.drawArc(c.rect, c.startAngle16, c.spanAngle16); break;
    }
    painter.end();
    return out;
}

QImage render(const QByteArray &doc, QRectF *usedViewBox = nullptr)
{
    QImage img(kCanvas, kCanvas, QImage::Format_ARGB32);
    img.fill(0);
    QSvgRenderer r(doc);
    if (usedViewBox) *usedViewBox = r.viewBoxF();
    QPainter p(&img);
    r.render(&p);
    return img;
}

int diffCount(const QImage &a, const QImage &b)
{
    int n = 0;
    for (int y = 0; y < kCanvas; ++y)
        for (int x = 0; x < kCanvas; ++x)
            if (a.pixel(x, y) != b.pixel(x, y)) ++n;
    return n;
}

std::string rowOf(const QImage &a, const QImage &b, int y)
{
    std::string s;
    for (int x = 0; x < kCanvas; ++x)
        s += (a.pixel(x, y) == b.pixel(x, y)) ? '.' : '#';
    return s;
}
} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) { std::cerr << "usage: probe <pkdocs.txt>\n"; return 2; }
    std::map<std::string, std::map<std::string, std::string>> docs;
    {
        std::ifstream in(argv[1]);
        std::string line;
        while (std::getline(in, line)) {
            auto t1 = line.find('\t'); auto t2 = line.find('\t', t1 + 1);
            if (t1 == std::string::npos || t2 == std::string::npos) continue;
            docs[line.substr(0, t1)][line.substr(t1 + 1, t2 - t1 - 1)] = line.substr(t2 + 1);
        }
    }

    int nVB = 0, nNoVB = 0, nCross = 0, total = 0;
    for (const auto &c : pkSvgCases::table()) {
        ++total;
        const auto &d = docs[c.name];
        QRectF vbPk, vbPkD, vbQt, vbQtNo;
        QImage qt       = render(renderQtDoc(c, true),  &vbQt);
        QImage qtNo     = render(renderQtDoc(c, false), &vbQtNo);
        QImage pk       = render(QByteArray::fromStdString(d.at("bounds")),  &vbPk);
        QImage pkD      = render(QByteArray::fromStdString(d.at("default")), &vbPkD);

        const int dWithVB = diffCount(qt, pk);
        const int dNoVB   = diffCount(qtNo, pkD);
        const int dCross  = diffCount(qt, pkD);

        if (dWithVB) ++nVB;
        if (dNoVB)   ++nNoVB;
        if (dCross)  ++nCross;

        if (dWithVB || dNoVB || dCross) {
            std::cout << "CASE " << c.name
                      << "  qtvb=" << vbQt.x() << "," << vbQt.y() << "," << vbQt.width() << "," << vbQt.height()
                      << "  pkvb=" << vbPk.x() << "," << vbPk.y() << "," << vbPk.width() << "," << vbPk.height()
                      << "  qtnovb=" << vbQtNo.x() << "," << vbQtNo.y() << "," << vbQtNo.width() << "," << vbQtNo.height()
                      << "  pkvbD=" << vbPkD.x() << "," << vbPkD.y() << "," << vbPkD.width() << "," << vbPkD.height()
                      << "  diff[qt-vs-pkbounds]=" << dWithVB
                      << " diff[qtNoVB-vs-pkdefault]=" << dNoVB
                      << " diff[qtVB-vs-pkdefault]=" << dCross
                      << "  sizes(qt=" << render(renderQtDoc(c,true)).size().width()
                      << ",qtNo=" << render(renderQtDoc(c,false)).size().width() << ")\n";
            std::cout << "  pkdefault doc: " << d.at("default") << "\n";
            if (dCross) {
                for (int y = 0; y < kCanvas; ++y) std::cout << "    " << rowOf(qt, pkD, y) << "\n";
            }
        }
    }
    std::cout << "SUMMARY total=" << total << " mism[bounds-vs-qtVB]=" << nVB
              << " mism[default-vs-qtNoVB]=" << nNoVB << " mism[default-vs-qtVB]=" << nCross << "\n";
    return 0;
}
