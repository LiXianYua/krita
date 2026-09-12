// R-64 隔离探针：把「6 位有效数字舍入」与「<ellipse> vs <path> 渲染路径」两个因素分开。
//
// 落点：pk/render/oracle/probes/isolate.cpp（Qt 侧）。由同目录 run_probes.sh 编译运行；它回答什么、期望读数见同目录 README.md。
#include <QBuffer>
#include <QByteArray>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QBrush>
#include <QRectF>
#include <QSvgRenderer>
#include <QSvgGenerator>
#include <iostream>
#include <string>

static const int kCanvas = 32;

static QByteArray gen(const std::string &d)
{
    std::string s = "<svg xmlns=\"http://www.w3.org/2000/svg\" xmlns:xlink=\"http://www.w3.org/1999/xlink\">"
                    "<defs></defs><g transform=\"matrix(1,0,0,1,0,0)\" opacity=\"1\"><path d=\"" + d +
                    "\" fill-rule=\"evenodd\" fill=\"none\" stroke=\"#000000\" stroke-opacity=\"1\" "
                    "stroke-width=\"2.5\" stroke-linecap=\"square\" stroke-linejoin=\"bevel\" "
                    "stroke-miterlimit=\"2\"/></g></svg>";
    return QByteArray::fromStdString(s);
}

static QByteArray genEllipse()
{
    QByteArray out; QBuffer b(&out); b.open(QIODevice::WriteOnly);
    QSvgGenerator g; g.setOutputDevice(&b); g.setResolution(72);
    QPainter p(&g); p.setRenderHint(QPainter::Antialiasing, true);
    p.setPen(QPen(Qt::black, 2.5)); p.setBrush(QBrush(Qt::NoBrush));
    p.drawEllipse(QRectF(2, 2, 1, 28));
    p.end();
    return out;
}

static std::string doc(const QByteArray &d) { return std::string(d.constData()); }

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
    const std::string d6 = "M3 16C3 23.732 2.77614 30 2.5 30C2.22386 30 2 23.732 2 16C2 8.26801 2.22386 2 2.5 2C2.77614 2 3 8.26801 3 16";
    // 全精度：2.7761423749153968 / 23.731986497631104 / 8.268013502368896 / 2.223857625084603
    const std::string d17 = "M3 16C3 23.731986497631104 2.7761423749153968 30 2.5 30C2.223857625084603 30 2 23.731986497631104 2 16C2 8.268013502368896 2.223857625084603 2 2.5 2C2.7761423749153968 2 3 8.268013502368896 3 16";
    // Pk 的 number() 口径 = setprecision(6)：与 generate 的 4 位不同，这里单独造一个 6 位版
    const std::string d6b = "M3 16C3 23.732 2.77614 30 2.5 30C2.22386 30 2 23.732 2 16C2 8.26801 2.22386 2 2.5 2C2.77614 2 3 8.26801 3 16";

    QRectF vb;
    QImage imgEllipse = render(genEllipse(), &vb);
    std::cout << "ellipse viewBox: " << vb.x() << "," << vb.y() << "," << vb.width() << "," << vb.height() << "\n";
    QImage imgPath6   = render(gen(d6), &vb);
    std::cout << "path6   viewBox: " << vb.x() << "," << vb.y() << "," << vb.width() << "," << vb.height() << "\n";
    QImage imgPath17  = render(gen(d17), &vb);
    std::cout << "path17  viewBox: " << vb.x() << "," << vb.y() << "," << vb.width() << "," << vb.height() << "\n";

    std::cout << "diff ellipse-vs-path6  = " << ndiff(imgEllipse, imgPath6)  << "\n";
    std::cout << "diff ellipse-vs-path17 = " << ndiff(imgEllipse, imgPath17) << "\n";
    std::cout << "diff path6-vs-path17   = " << ndiff(imgPath6, imgPath17)   << "\n";
    (void)d6b;
    return 0;
}
