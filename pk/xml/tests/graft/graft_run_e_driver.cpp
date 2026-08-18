// R-25 Task 3 判据②候选 E（libs/pigment/resources/KoColorSet.cpp 的降级试探）：
//
// 真实调用点用量落在 KoColorSet.cpp 两个函数——`loadSbzSwatchbook()`（DOM 侧
// `QDomElement::lineNumber()`/`columnNumber()`，行 1953-2013）与 `loadXml()`
// （Stream 侧 `QXmlStreamReader::lineNumber()`/`columnNumber()`，行
// 2436-2439）。`libs/pigment/tests/TestKoColorSet.cpp` 的 `testLoadXML()`/
// `testLoadSBZ()`（分别加载 `scribus.xml`/`swatchbook.sbz` 测试夹具）确实会
// 跑到这两个函数——是本能力真正被压到的真实调用点，但 R-25 计划 Task 3
// checklist 原先设想的"整个 KoColorSet.cpp 被编译进这个测试可执行文件"这条
// 编译期证据路径不成立：`libs/pigment/tests/CMakeLists.txt` 里
// `kis_add_tests(TestKoColorSet.cpp ... LINK_LIBRARIES kritapigment ...)` 是
// 链接已构建好的 `kritapigment` 库，不是把 `KoColorSet.cpp` 源文件一起编进
// 测试可执行文件。真正编译 `KoColorSet.cpp`（连带它的 37 处
// lineNumber()/columnNumber() 调用）的是 `kritapigment` 这个库自己的 CMake
// 目标，不在 pk/xml 的 `locks` 范围内。
//
// 对 `KoColorSet.cpp` 直接 `-fsyntax-only` 试探（现场命令与输出见
// graft_run_e.sh 头注释）：
//   1. 缺 `kritapigment_export.h`（libs/pigment 整库需要自己的 CMake
//      generate_export_header() 才产出——同 R-25 task-1-report.md 候选 A/B
//      撞的同一堵墙）；
//   2. stub 掉这一层继续编译还会撞 `klocalizedstring.h`（KF5::I18n 依赖）——
//      墙比 Task 1 报告记录的更深，不是"补几个 stub"能在时间盒内解决的规模，
//      且 `libs/pigment`/KF5 依赖整体不在本任务范围。
//
// 退化到与 Task 1/2 同款处理方式（README §11.3 / 计划设计①-b 的先例）：
// 逐字复刻 KoColorSet.cpp 两处真实调用点用到 lineNumber()/columnNumber()
// 的调用形状（类型、成员访问方式全部对齐），只用 pk/xml 自己已交付的
// compat 垫片编译——只证明"签名字面兼容，行为数值正确"，不证明整份
// KoColorSet.cpp 能编译。
//
// 原文① DOM 侧（未改一个字，逐字抄自 loadSbzSwatchbook()，
// libs/pigment/resources/KoColorSet.cpp:1934/1949-1955）：
//
//   QDomElement root = doc.documentElement(); // SwatchBook
//   ...
//   QDomElement book = root.firstChildElement("book");
//   if (book.isNull()) {
//       warnPigment << "Palette book (swatch composition) not found (line" << root.lineNumber()
//                   << ", column" << root.columnNumber()
//                   << ")";
//       return false;
//   }
//
// 原文② Stream 侧（未改一个字保留调用形状，逐字抄自 loadXml()，
// libs/pigment/resources/KoColorSet.cpp:2418-2439，`data` 是真实调用点里的
// `KoColorSet::Private::data`，类型 `QByteArray`——`QXmlStreamReader` 有
// 接受 `QByteArray` 的隐式构造重载，但本 worktree 没有交付 `PkByteArray`
// （R-02），`PkXmlStreamReader` 也只有 `PkString` 构造重载（Task 2 交付）
// ——这是已经独立记录过的范围边界（README §11.3 / Task 2 计划设计①-c），
// 跟本任务的 lineNumber()/columnNumber() 无关，下面的形状对齐版本改用
// PkString 构造，其余调用形状逐字保留）：
//
//   QXmlStreamReader *xml = new QXmlStreamReader(data);
//   if (xml->readNextStartElement()) {
//       auto paletteId = xml->name();
//       if (paletteId.compare(QString("SCRIBUSCOLORS"), Qt::CaseInsensitive) == 0) {
//           res = loadScribusXmlPalette(colorSet, xml);
//       }
//       else {
//           xml->raiseError("Unknown XML palette format. Expected SCRIBUSCOLORS, found " + paletteId);
//       }
//   }
//   if (xml->hasError() || !res) {
//       warnPigment << "Illegal XML palette:" << colorSet->filename();
//       warnPigment << "Error (line"<< xml->lineNumber() << ", column" << xml->columnNumber() << "):" << xml->errorString();
//       return false;
//   }
//
// 下面两个 *Shape() 函数是这两段代码的形状对齐版本——去掉了
// `paletteId.compare(..., Qt::CaseInsensitive)`（本 worktree 没有
// `Qt::CaseInsensitive` 的 compat 映射，不是本任务要验证的对象，换成
// PkString 的 `==`）与 `loadScribusXmlPalette()`/`colorSet->filename()`
// （生产胶水，不是本任务要验证的对象），其余 `QDomElement`/
// `.firstChildElement(`/`.isNull()`/`.lineNumber()`/`.columnNumber()`
// / `QXmlStreamReader`/`.readNextStartElement()`/`.name()`/`.raiseError(`/
// `.hasError()`/`.lineNumber()`/`.columnNumber()`/`.errorString()` 的类型与
// 调用形状逐字保留。

#include <QDomDocument>
#include <QDomElement>
#include <QString>
#include <QXmlStreamReader>

#include <cassert>
#include <cstdio>

bool loadSbzSwatchbookLineColShape(const QDomDocument &doc)
{
    QDomElement root = doc.documentElement(); // SwatchBook

    QDomElement book = root.firstChildElement("book");
    if (book.isNull()) {
        std::printf("Palette book (swatch composition) not found (line %d, column %d)\n",
                     root.lineNumber(), root.columnNumber());
        return false;
    }
    return true;
}

bool loadXmlLineColShape(const QString &data)
{
    bool res = false;

    QXmlStreamReader *xml = new QXmlStreamReader(data);

    if (xml->readNextStartElement()) {
        auto paletteId = xml->name();
        if (paletteId == QString("SCRIBUSCOLORS")) { // Scribus
            res = true; // 简化掉 loadScribusXmlPalette()，不是本试探要验证的对象
        }
        else {
            // Unknown XML format——真实调用点用 `"literal" + paletteId`
            // （`QString::operator+` 是自由函数，隐式转换字面量），PkString
            // 只提供成员 `operator+(const PkString&)`（PkString.h:56），左操作数
            // 是裸字符串字面量时不会触发隐式转换走成员重载——不是本任务要
            // 验证的对象（这里验证的是 raiseError()/lineNumber()/
            // columnNumber() 的调用形状），改成显式 `PkString(...) + paletteId`
            // 拼接，`raiseError(const PkString&)` 本身的调用形状不变。
            xml->raiseError(PkString("Unknown XML palette format. Expected SCRIBUSCOLORS, found ")
                             + paletteId);
        }
    }

    bool ok = true;
    if (xml->hasError() || !res) {
        std::printf("Illegal XML palette. Error (line %d, column %d): %s\n",
                     xml->lineNumber(), xml->columnNumber(), xml->errorString().PkToUtf8().c_str());
        ok = false;
    }
    delete xml;
    return ok;
}

int main()
{
    // ① DOM 侧：root 是真实解析出来的元素（探针 P15 黄金数据同款输入的一个
    // 子集——<root> 单标签场景已在 test_node.cpp 精确验证过数值，这里只需要
    // 确认真实调用点的调用形状编译期兼容 + 运行期不崩溃/返回非负行列）。
    QDomDocument doc;
    const bool parsed = doc.setContent(QString("<SwatchBook><metadata/></SwatchBook>"));
    assert(parsed);
    const bool r1 = loadSbzSwatchbookLineColShape(doc); // book 子元素不存在，走 lineNumber() 分支
    const bool domOk = !r1; // 预期返回 false（book 缺失）

    // ② Stream 侧：复刻探针 P16 的 UNKNOWNROOT 场景，line=1 col=22 已经在
    // test_stream_reader.cpp 精确验证过，这里只需要确认调用形状编译期兼容。
    const bool r2 = loadXmlLineColShape(QString("<UNKNOWNROOT attr='1'></UNKNOWNROOT>"));
    const bool streamOk = !r2; // 预期返回 false（未知根元素名，raiseError）

    const bool ok = domOk && streamOk;
    std::printf(ok ? "shape-call OK\n" : "shape-call MISMATCH\n");
    return ok ? 0 : 1;
}
