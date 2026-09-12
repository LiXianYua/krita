// R-64 全表对照：Qt 自己的 QSvgGenerator 走 **与 Pk 后端收到的同一套命令流**
// （椭圆 drawEllipse / 多边形 addPolygon+closeSubpath+drawPath / 弧 arcMoveTo+arcTo+drawPath），
// 默认 generator 设置（无 width/height/viewBox），与 Pk default-ctor 文档逐像素比。
//
// R-64 修复轮 1 订正：椭圆侧原先让 Qt 改道走 addEllipse+drawPath（于是比对的是
// 「Pk 的 path 输出 = Qt 的 drawPath 输出」——对齐到**未被替换**的入口，是全分支评审 B-1
// 点名的循环论证）。现改为 Qt 椭圆也走 drawEllipse（Qt 自己发 <ellipse>），与 Pk 后端
// 收到的同一命令对齐 —— 这才是替换契约本身。多边形/弧仍走 path 形态（Pk 后端对它们
// 仍发 <path>）。
//
// 落点：pk/render/oracle/probes/mirror.cpp（Qt 侧）。由同目录 run_probes.sh 编译运行；它回答什么、期望读数见同目录 README.md。
#include "svg_primitive_cases.h"

#include <QBuffer>
#include <QBrush>
#include <QByteArray>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QPolygonF>
#include <QRectF>
#include <QSvgGenerator>
#include <QSvgRenderer>

#include <fstream>
#include <iostream>
#include <map>
#include <string>

static const int kCanvas = 32;

static QByteArray mirrorDoc(const pkSvgCases::Case &c)
{
    QByteArray out; QBuffer b(&out); b.open(QIODevice::WriteOnly);
    QSvgGenerator g; g.setOutputDevice(&b); g.setResolution(72);
    QPainter p(&g); p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(c.hasPen ? QPen(Qt::black, c.penWidth) : QPen(Qt::NoPen));
    p.setBrush(c.hasBrush ? QBrush(Qt::red) : QBrush(Qt::NoBrush));
    switch (c.kind) {
    case pkSvgCases::Kind::Ellipse: {
        // 与 Pk 后端收到的同一条命令：drawEllipse（Qt 发 <ellipse>/<circle>，与 Pk 同形）。
        p.drawEllipse(c.rect); break; }
    case pkSvgCases::Kind::Polygon: {
        QPainterPath path; QPolygonF poly;
        for (const auto &q : c.points) poly << q;
        path.addPolygon(poly); path.closeSubpath(); p.drawPath(path); break; }
    case pkSvgCases::Kind::Arc: {
        const QRectF r = c.rect.normalized();
        QPainterPath path;
        path.arcMoveTo(r, c.startAngle16 / 16.0);
        path.arcTo(r, c.startAngle16 / 16.0, c.spanAngle16 / 16.0);
        p.setBrush(QBrush(Qt::NoBrush));
        p.drawPath(path); break; }
    }
    p.end();
    return out;
}

static int ndiff(const QImage &a, const QImage &b)
{
    int n = 0;
    for (int y = 0; y < kCanvas; ++y) for (int x = 0; x < kCanvas; ++x) if (a.pixel(x,y) != b.pixel(x,y)) ++n;
    return n;
}

static QImage render(const QByteArray &d)
{
    QImage img(kCanvas, kCanvas, QImage::Format_ARGB32); img.fill(0);
    QSvgRenderer r(d);
    QPainter p(&img); r.render(&p);
    return img;
}

int main(int argc, char **argv)
{
    if (argc < 2) { std::cerr << "usage: mirror <pkdocs.txt>\n"; return 2; }
    std::map<std::string, std::map<std::string, std::string>> docs;
    { std::ifstream in(argv[1]); std::string line;
      while (std::getline(in, line)) { auto t1 = line.find('\t'); auto t2 = line.find('\t', t1+1);
        if (t1 == std::string::npos || t2 == std::string::npos) continue;
        docs[line.substr(0,t1)][line.substr(t1+1, t2-t1-1)] = line.substr(t2+1); } }

    int total = 0, mism = 0;
    for (const auto &c : pkSvgCases::table()) {
        ++total;
        const int d = ndiff(render(mirrorDoc(c)), render(QByteArray::fromStdString(docs[c.name].at("default"))));
        if (d) { ++mism; std::cout << "MISMATCH " << c.name << " diff=" << d << "\n"
                                   << "  Qt mirror: " << mirrorDoc(c).constData() << "\n"
                                   << "  Pk default: " << docs[c.name].at("default") << "\n"; }
    }
    std::cout << "MIRROR total=" << total << " mismatch=" << mism << "\n";
    return 0;
}
