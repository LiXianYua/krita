// Qt 侧**比较器**（脚本手工编，唯一链真 Qt5Svg 的一步；不进任何产物）。
//
// 用法：svg_primitive_qt <pk-output.txt> [<golden-body.txt>]
//   argv[1] = svg_primitive_oracle_pk 的 stdout（每行 `<用例名>\t<Pk 文档正文>`）
//   argv[2] = 可选；写出金标正文（每行 `<名字> doc=<...> qt=<...> pk=<...>`）
//
// 协议（R线-spec〈形态契约〉，逐字）：
//   · stdout 只有两种行：
//       `DIFF total=<N> mismatch=<M>`（恰一行）
//       `DIFFTAG <api> <tag> <count>`（每个不一致的用例一行）
//   · 退出码恒 0（mismatch>0 也不例外——判定差异该不该存在是 reviewer 的事）
//
// 比较面（R-54 §1.3，真 Qt 探针实测逼出来的）：两份 SVG 文档的**文本按定义不同**
// （Qt 对 drawEllipse 产出 `<ellipse cx cy rx ry>`、数字格式/属性集合/顺序全不同），
// 所以不能文本 diff。改为**同一个渲染器**（真 Qt QSvgRenderer）把两侧各自的文档渲染成
// 同尺寸 32×32 ARGB32 图后逐像素比——比较面里唯一变化的是文档内容，渲染器差异被消掉。
// 本仓同一个后端的先例：libs/flake/flake/tests/PkSvgPainterBackendTest.cpp:63-71。
//
// `total` = 用例数；`mismatch` = 渲染后**有任何像素不同**的用例数。DIFFTAG 的 count =
// 该用例不同的像素数（tag 由该用例的输入形态构造，见 tagFor——R线-spec〈规则一〉：
// tag 必须由触发差异的输入形态参与构造，不能是常量字面量）。
#include "svg_primitive_cases.h"

#include <QBuffer>
#include <QBrush>
#include <QByteArray>
#include <QColor>
#include <QImage>
#include <QPainter>
#include <QPen>
#include <QPolygonF>
#include <QRect>
#include <QSize>
#include <QSvgGenerator>
#include <QSvgRenderer>

#include <cstdint>
#include <cstdlib>
#include <fstream>
#include <iostream>
#include <map>
#include <sstream>
#include <string>
#include <vector>

namespace {

constexpr int kCanvas = 32;

QByteArray renderReferenceDoc(const pkSvgCases::Case &c)
{
    QByteArray out;
    QBuffer buffer(&out);
    buffer.open(QIODevice::WriteOnly);
    QSvgGenerator generator;
    generator.setOutputDevice(&buffer);
    generator.setSize(QSize(kCanvas, kCanvas));
    generator.setViewBox(QRect(0, 0, kCanvas, kCanvas));
    generator.setResolution(72);
    QPainter painter(&generator);
    painter.setRenderHint(QPainter::Antialiasing, true);
    painter.setPen(c.hasPen ? QPen(Qt::black, c.penWidth) : QPen(Qt::NoPen));
    painter.setBrush(c.hasBrush ? QBrush(Qt::red) : QBrush(Qt::NoBrush));
    switch (c.kind) {
    case pkSvgCases::Kind::Ellipse:
        painter.drawEllipse(c.rect);
        break;
    case pkSvgCases::Kind::Polygon: {
        QPolygonF polygon;
        for (const auto &p : c.points) polygon << p;
        painter.drawPolygon(polygon);
        break;
    }
    case pkSvgCases::Kind::Arc:
        painter.drawArc(c.rect, c.startAngle16, c.spanAngle16);
        break;
    }
    painter.end();
    return out;
}

// FNV-1a 32，逐字节；与 test_svg_primitive.cpp 的 doc 摘要必须逐位一致。
std::uint32_t fnv1a(const std::string &bytes)
{
    std::uint32_t hash = 2166136261u;
    for (unsigned char byte : bytes) {
        hash ^= byte;
        hash *= 16777619u;
    }
    return hash;
}

// 与 shape_primitive_cases.h 的 digest 同口径：按 pixel() 的打包 0xAARRGGBB 逐像素做。
std::uint32_t pixelDigest(const QImage &image)
{
    std::uint32_t hash = 2166136261u;
    const auto mix = [&hash](std::uint8_t byte) {
        hash ^= byte;
        hash *= 16777619u;
    };
    for (int y = 0; y < image.height(); ++y) {
        for (int x = 0; x < image.width(); ++x) {
            const std::uint32_t value = image.pixel(x, y);
            mix(std::uint8_t((value >> 24) & 0xff));
            mix(std::uint8_t((value >> 16) & 0xff));
            mix(std::uint8_t((value >> 8) & 0xff));
            mix(std::uint8_t(value & 0xff));
        }
    }
    return hash;
}

std::string hex32(std::uint32_t value)
{
    std::ostringstream out;
    out << std::hex << value;
    return out.str();
}

const char *apiName(pkSvgCases::Kind kind)
{
    switch (kind) {
    case pkSvgCases::Kind::Ellipse: return "drawEllipse";
    case pkSvgCases::Kind::Polygon: return "drawPolygon";
    case pkSvgCases::Kind::Arc: return "drawArc";
    }
    return "drawUnknown";
}

// tag 由输入形态参与构造（R线-spec〈规则一〉）。
std::string tagFor(const pkSvgCases::Case &c)
{
    std::ostringstream out;
    out << apiName(c.kind)
        << ":rect=" << c.rect.x() << "," << c.rect.y()
        << "," << c.rect.width() << "," << c.rect.height();
    if (c.kind == pkSvgCases::Kind::Polygon) out << ":pts=" << c.points.size();
    if (c.kind == pkSvgCases::Kind::Arc) out << ":s" << c.startAngle16 << ":w" << c.spanAngle16;
    out << ":pen=" << (c.hasPen ? "2.5" : "none") << ":brush=" << (c.hasBrush ? "red" : "none");
    return out.str();
}

} // namespace

int main(int argc, char **argv)
{
    if (argc < 2) {
        std::cerr << "usage: svg_primitive_qt <pk-output.txt> [<golden-body.txt>]\n";
        return 2;
    }

    std::map<std::string, std::string> pkDocs;
    {
        std::ifstream in(argv[1]);
        if (!in) {
            std::cerr << "cannot open pk output: " << argv[1] << '\n';
            return 2;
        }
        for (std::string line; std::getline(in, line);) {
            const auto tab = line.find('\t');
            if (tab == std::string::npos) continue;
            pkDocs[line.substr(0, tab)] = line.substr(tab + 1);
        }
    }

    std::vector<std::string> goldenBody;
    // 空画布摘要：与某用例渲染后的 qt 摘要相同 = 该用例两侧都没画东西（恒真、无判别力）。
    // 由 Qt 侧测出、随金标以 `# blank=` 附给 Qt-free 的 ctest，别让它在无渲染能力的一侧凭空造。
    QImage blankImage(kCanvas, kCanvas, QImage::Format_ARGB32);
    blankImage.fill(0);
    const std::uint32_t blankDigest = pixelDigest(blankImage);

    int total = 0;
    int mismatch = 0;
    for (const auto &c : pkSvgCases::table()) {
        ++total;
        const auto pkIt = pkDocs.find(c.name);
        const std::string pkDoc = pkIt == pkDocs.end() ? std::string() : pkIt->second;

        const QByteArray reference = renderReferenceDoc(c);
        QSvgRenderer refRenderer(reference);
        QSvgRenderer pkRenderer(QByteArray::fromStdString(pkDoc));

        QImage refImage(kCanvas, kCanvas, QImage::Format_ARGB32);
        QImage pkImage(kCanvas, kCanvas, QImage::Format_ARGB32);
        refImage.fill(0);
        pkImage.fill(0);
        {
            QPainter refPainter(&refImage);
            QPainter pkPainter(&pkImage);
            refRenderer.render(&refPainter);
            pkRenderer.render(&pkPainter);
        }

        int differing = 0;
        for (int y = 0; y < kCanvas; ++y) {
            for (int x = 0; x < kCanvas; ++x) {
                if (refImage.pixel(x, y) != pkImage.pixel(x, y)) ++differing;
            }
        }
        if (differing != 0) {
            ++mismatch;
            std::cout << "DIFFTAG " << apiName(c.kind) << ' ' << tagFor(c)
                      << ' ' << differing << '\n';
        }

        goldenBody.push_back(c.name + " doc=" + hex32(fnv1a(pkDoc)) +
                             " qt=" + hex32(pixelDigest(refImage)) +
                             " pk=" + hex32(pixelDigest(pkImage)));
    }

    std::cout << "DIFF total=" << total << " mismatch=" << mismatch << '\n';

    if (argc >= 3) {
        std::ofstream out(argv[2]);
        if (!out) {
            std::cerr << "cannot write golden body: " << argv[2] << '\n';
            return 2;
        }
        for (const auto &line : goldenBody) out << line << '\n';
        out << "# blank=" << hex32(blankDigest) << '\n';
    }

    return 0;  // 恒 0，即使 mismatch > 0（R线-spec〈对拍怎么做·形态契约〉）
}
