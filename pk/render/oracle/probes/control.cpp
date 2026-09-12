// R-64 判别力对照：Qt **自己的** QSvgGenerator 走 drawPath 时，其文档在同一个隐式
// viewBox 拉伸下是否也出现同样的 4 px？若是，则 4 px 是「6 位有效数字的 SVG 路径序列化」
// 的固有限制，不是 Pk 的偏离。
//
// 落点：pk/render/oracle/probes/control.cpp（Qt 侧）。由同目录 run_probes.sh 编译运行；它回答什么、期望读数见同目录 README.md。
#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QPainter>
#include <QPainterPath>
#include <QPen>
#include <QBrush>
#include <QRectF>
#include <QSvgGenerator>
#include <QSvgRenderer>
#include <iostream>
#include <string>

static const int kCanvas = 32;

static QByteArray genDoc(bool usePath)
{
    QByteArray out; QBuffer b(&out); b.open(QIODevice::WriteOnly);
    QSvgGenerator g; g.setOutputDevice(&b); g.setResolution(72);
    QPainter p(&g); p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(Qt::black, 2.5)); p.setBrush(QBrush(Qt::NoBrush));
    if (!usePath) { p.drawEllipse(QRectF(2, 2, 1, 28)); }
    else { QPainterPath path; path.addEllipse(QRectF(2, 2, 1, 28)); p.drawPath(path); }
    p.end();
    return out;
}

static int ndiff(const QImage &a, const QImage &b)
{
    int n = 0;
    for (int y = 0; y < kCanvas; ++y) for (int x = 0; x < kCanvas; ++x) if (a.pixel(x,y) != b.pixel(x,y)) ++n;
    return n;
}

static QImage render(const QByteArray &d, QRectF *vb = nullptr)
{
    QImage img(kCanvas, kCanvas, QImage::Format_ARGB32); img.fill(0);
    QSvgRenderer r(d); if (vb) *vb = r.viewBoxF();
    QPainter p(&img); r.render(&p);
    return img;
}

int main()
{
    const QByteArray ellipse = genDoc(false);
    const QByteArray pathDoc = genDoc(true);
    std::cout << "=== Qt drawEllipse doc ===\n" << ellipse.constData() << "\n";
    std::cout << "=== Qt drawPath doc ===\n" << pathDoc.constData() << "\n";

    QRectF v1, v2;
    const QImage i1 = render(ellipse, &v1);
    const QImage i2 = render(pathDoc, &v2);
    std::cout << "viewBox ellipse: " << v1.x() << "," << v1.y() << "," << v1.width() << "," << v1.height() << "\n";
    std::cout << "viewBox path   : " << v2.x() << "," << v2.y() << "," << v2.width() << "," << v2.height() << "\n";
    std::cout << "diff Qt(drawEllipse) vs Qt(drawPath) under implicit stretch = " << ndiff(i1, i2) << "\n";
    return 0;
}
