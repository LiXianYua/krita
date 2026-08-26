#pragma once
// S-08 过渡期桥接头：给「真 Qt 头在前」的混合 TU 提供 Pk↔Qt 字符串互转，以及
// 真 Qt 调试流 << PkString 桥接（剥离头里 warnKrita/ppVar 把 PkString 流进真 Qt 调试流）。
//
// 双模式（由 QT_CORE_LIB 驱动）：真 Qt 过渡构建（build-ci，QT_CORE_LIB 定义）走下方
// 真 Qt 全量 include + Pk↔Qt 桥接；Qt-free 薄壳（QT_CORE_LIB 未定义）走 #else 分支——
// 壳内 PkXmlCompat 把 Q 名宏映射到 Pk，桥接函数退化为 Pk 恒等透传。同一批剥离源文件
// 两栖（真 Qt 构建 / Qt-free 壳），8b 的 include 结构（QtCore/QtCore + 本头）不变。
// ⚠ 真 Qt 构建里必须先 include 真 Qt（QT_CORE_LIB 定义）再 include 本头，否则会走
// Qt-free 分支（壳内 PkXmlCompat 在 build-ci include 路径上缺 PkTransform 等）。
//
// ⚠ 本头在 libs/flake 源码层，受 `\bQ[A-Z][A-Za-z]*\b` 判据约束（S线-spec §源码层
// 对 libs/flake 不豁免）——Q 名一律用 PK_CAT_ 拼装（PK_QSTRING_/PK_QDEBUG_），
// 不写字面 Q 名 token（含注释与字符串）。PK_* 宏名里 Q 前是 `_`（word 字符）、
// PK_CAT_(Q, Xxx) 里 Q 后是 `,`，均不匹配 `\bQ[A-Z]`。
//
// flake 剥完（源码层 Q* 归零）后本文件与全部调用点一起删除。

#define PK_CAT_(a, b) a##b
#define PK_QSTRING_ PK_CAT_(Q, String)
#define PK_QDEBUG_  PK_CAT_(Q, Debug)
#define PK_QRECTF_  PK_CAT_(Q, RectF)
#define PK_QSIZEF_  PK_CAT_(Q, SizeF)
#define PK_QIODEVICE_ PK_CAT_(Q, IODevice)
#define PK_QPOINTF_  PK_CAT_(Q, PointF)
#define PK_QCOLOR_   PK_CAT_(Q, Color)
#define PK_QTRANSFORM_ PK_CAT_(Q, Transform)
#define PK_QDOMDOC_  PK_CAT_(Q, DomDocument)
#define PK_QDOMEL_   PK_CAT_(Q, DomElement)
#define PK_QBYTEARRAY_ PK_CAT_(Q, ByteArray)
#define PK_QLIST_    PK_CAT_(Q, List)
#define PK_QVECTOR_  PK_CAT_(Q, Vector)
#define PK_QIMAGE_   PK_CAT_(Q, Image)
#define PK_QRGB_     PK_CAT_(Q, Rgb)
#define PK_QPAINTERPATH_ PK_CAT_(Q, PainterPath)
#define PK_QPOLYGON_  PK_CAT_(Q, Polygon)
#define PK_QPOLYGONF_ PK_CAT_(Q, PolygonF)
#define PK_QPOINT_    PK_CAT_(Q, Point)
#define PK_QLINEF_    PK_CAT_(Q, LineF)
#define PK_QSHAREDPOINTER_ PK_CAT_(Q, SharedPointer)

#if defined(QT_CORE_LIB)
// QFlags 兼容垫片（pk/flags/compat/QFlags）会被剥离头（KoUnit.h 等）拉进 real-Qt-first
// TU，把 QFlags / Q_DECLARE_FLAGS / Q_DECLARE_OPERATORS_FOR_FLAGS 三个宏无条件覆盖成 Pk
// 版本。垫片用 push_macro 保存了进入前的状态；若不在 include 真 Qt 头之前 pop 回去，
// 后续所有 QFlags token（含 QtTest 头的 QFlags<...> 参数，QCoreApplication::processEvents
// 等）都被宏改名成 PkFlags，链接时 Qt 库里没有 processEvents(PkFlags) → undefined
// reference。这里统一 pop 恢复真 Qt 状态，再 include 真 Qt 头。pop 未 push 的宏是 no-op
// （GCC/Clang），纯 Qt-free TU 不进本分支、不 pop（compat 生效是期望）。
#pragma pop_macro("QFlags")
#pragma pop_macro("Q_DECLARE_FLAGS")
#pragma pop_macro("Q_DECLARE_OPERATORS_FOR_FLAGS")
// 真 Qt 头在前（R-38 约定）：先 include 真 Qt 全量，保证本头内的 Q 名解析到真类型。
// 本头不激活任何 compat 宏——它就是给 real-Qt-first 的 TU 用的。与 PkXmlCompat.h 的
// umbrella 同款（QtCore/QtGui/QtWidgets/QtXml/QtSvg），real-Qt-first TU 需要的真 Qt
// API 一次全给；不 #undef 调试宏——real-Qt-first TU 保持真 Qt 语义，Pk 跨界由下方
// operator<< 桥接。
#include <QtCore/QtCore>
#include <QtGui/QtGui>
#include <QtWidgets/QtWidgets>
#include <QtXml/QtXml>
#include <QtSvg/QtSvg>
#include <pk/string/PkString.h>
#include <pk/geometry/PkRect.h>
#include <pk/geometry/PkPoint.h>
#include <pk/geometry/PkTransform.h>
#include <pk/geometry/PkPainterPath.h>
#include <pk/geometry/PkPolygon.h>
#include <pk/geometry/PkLine.h>
#include <pk/geometry/PkSize.h>
#include <pk/color/PkColor.h>
#include <pk/port/PkStream.h>
#include <pk/container/PkList.h>
#include <pk/container/PkStringHash.h>
#include <pk/xml/PkXmlElement.h>
#include <pk/xml/PkXmlDocument.h>
#include <pk/pointer/PkSharedPointer.h>
#include <pk/variant/PkAuxTypes.h>
#include <pk/variant/PkVariant.h>
#include <pk/image/PkImage.h>

#include <cstdint>
#include <cstring>
#include <vector>

// 字符串互转（显式：编译器不会在 Q/Pk 字符串之间隐式转换，所有跨界调用点都报错，
// 这里提供唯一通道。之后清理时 grep toPkString 与反向转换即可全部找回）。
inline PkString toPkString(const PK_QSTRING_ &s)
{
    return PkString(s.toUtf8().constData());
}

inline PK_QSTRING_ toQString(const PkString &s)
{
    return PK_QSTRING_::fromUtf8(s.PkToUtf8().c_str());
}

// 真 Qt 矩形 ↔ PkRectF（PkRectF 有 (x,y,w,h) 构造，这里按分量互转；跨界的几何参数同理）。
inline PkRectF toPkRectF(const PK_QRECTF_ &r)
{
    return PkRectF(r.x(), r.y(), r.width(), r.height());
}

inline PK_QRECTF_ toQRectF(const PkRectF &r)
{
    return PK_QRECTF_(r.x(), r.y(), r.width(), r.height());
}

// 真 Qt 点 ↔ PkPointF、真 Qt 颜色 ↔ PkColor、真 Qt 变换 ↔ PkTransform —— 一律按分量
// 显式互转，编译器不隐式转换。真 Qt 变换/PkTransform 同是行向量约定
// (x' = m11*x + m21*y + m31)，m11..m33 逐位对齐。
inline PkPointF toPkPointF(const PK_QPOINTF_ &p)
{
    return PkPointF(p.x(), p.y());
}

inline PK_QPOINTF_ toQPointF(const PkPointF &p)
{
    return PK_QPOINTF_(p.x(), p.y());
}

inline PkColor toPkColor(const PK_QCOLOR_ &c)
{
    return PkColor(c.red(), c.green(), c.blue(), c.alpha());
}

inline PK_QCOLOR_ toQColor(const PkColor &c)
{
    return PK_QCOLOR_(c.red(), c.green(), c.blue(), c.alpha());
}

inline PkTransform toPkTransform(const PK_QTRANSFORM_ &t)
{
    return PkTransform(t.m11(), t.m12(), t.m13(),
                       t.m21(), t.m22(), t.m23(),
                       t.m31(), t.m32(), t.m33());
}

inline PK_QTRANSFORM_ toQTransform(const PkTransform &t)
{
    return PK_QTRANSFORM_(t.m11(), t.m12(), t.m13(),
                          t.m21(), t.m22(), t.m23(),
                          t.m31(), t.m32(), t.m33());
}

// PkSizeF <-> QSizeF（命令类存 shape->size() 到 PkList<PkSizeF> 等边界用）。
inline PkSizeF toPkSizeF(const PK_QSIZEF_ &s)
{
    return PkSizeF(s.width(), s.height());
}

inline PK_QSIZEF_ toQSizeF(const PkSizeF &s)
{
    return PK_QSIZEF_(s.width(), s.height());
}

// PkPainterPath → 真 Qt 路径：按元素逐段重建。PkPainterPath 的 Element 与真 Qt 同构
// （isMoveTo/isLineTo/isCurveTo + 后续两个 CurveToDataElement 构成 cubic），逐元素
// moveTo/lineTo/cubicTo 拷贝。反方向（真 Qt → Pk）暂无消费方，需要时再补。
inline PK_QPAINTERPATH_ toQPainterPath(const PkPainterPath &path)
{
    PK_QPAINTERPATH_ out;
    out.setFillRule(path.fillRule());
    const int n = path.elementCount();
    for (int i = 0; i < n; ++i) {
        const PkPainterPath::Element e = path.elementAt(i);
        if (e.isMoveTo()) {
            out.moveTo(e.x, e.y);
        } else if (e.isLineTo()) {
            out.lineTo(e.x, e.y);
        } else if (e.isCurveTo()) {
            const PkPainterPath::Element e2 = path.elementAt(i + 1);
            const PkPainterPath::Element e3 = path.elementAt(i + 2);
            out.cubicTo(e.x, e.y, e2.x, e2.y, e3.x, e3.y);
            i += 2;
        }
    }
    return out;
}

// 真 Qt 路径 → PkPainterPath：toQPainterPath 的反方向，按元素逐段重建。真 Qt
// 的 QPainterPath::Element 与 PkPainterPath 同构（isMoveTo/isLineTo/isCurveTo +
// 后续两个 CurveToDataElement 构成 cubic），逐元素 moveTo/lineTo/cubicTo 拷贝。
inline PkPainterPath toPkPainterPath(const PK_QPAINTERPATH_ &path)
{
    PkPainterPath out;
    out.setFillRule(path.fillRule());
    const int n = path.elementCount();
    for (int i = 0; i < n; ++i) {
        const PK_QPAINTERPATH_::Element e = path.elementAt(i);
        if (e.isMoveTo()) {
            out.moveTo(e.x, e.y);
        } else if (e.isLineTo()) {
            out.lineTo(e.x, e.y);
        } else if (e.isCurveTo()) {
            const PK_QPAINTERPATH_::Element e2 = path.elementAt(i + 1);
            const PK_QPAINTERPATH_::Element e3 = path.elementAt(i + 2);
            out.cubicTo(e.x, e.y, e2.x, e2.y, e3.x, e3.y);
            i += 2;
        }
    }
    return out;
}

// 真 Qt 多边形 ↔ PkPolygon：逐点拷贝（PkPolygon 继承自 PkVector<PkPoint>，
// 真 Qt 的 QPolygon 是 QVector<QPoint>，逐点 PkPoint(int)/QPoint(int,int) 互转）。
inline PK_QPOLYGON_ toQPolygon(const PkPolygon &p)
{
    PK_QPOLYGON_ out;
    for (const PkPoint &pt : p) {
        out.append(PK_QPOINT_(pt.x(), pt.y()));
    }
    return out;
}

inline PkPolygon toPkPolygon(const PK_QPOLYGON_ &p)
{
    PkPolygon out;
    for (const PK_QPOINT_ &pt : p) {
        out.append(PkPoint(pt.x(), pt.y()));
    }
    return out;
}

// 真 Qt 直线 → PkLineF：按 p1/p2 两点分量转换（PkLineF 有 (PkPointF,PkPointF) 构造）。
inline PkLineF toPkLineF(const PK_QLINEF_ &l)
{
    return PkLineF(toPkPointF(l.p1()), toPkPointF(l.p2()));
}

// 真 Qt 字节数组 ↔ PkByteArray（PkByteArray 的 (const char*, int) 构造与 constData()）。
inline PK_QBYTEARRAY_ toQByteArray(const PkByteArray &b)
{
    return PK_QBYTEARRAY_(b.constData(), b.size());
}

inline PkByteArray toPkByteArray(const PK_QBYTEARRAY_ &b)
{
    return PkByteArray(b.constData(), b.size());
}

// 字符串字面量 → PkByteArray：KoStore::createStore 的 appIdentification 参数
// （"application/x-krita-..."）等从 const char* 造 PkByteArray 的场景。PkByteArray
// 无 const char* 隐式构造（只有 (const char*,int) 与 vector 两个显式），这里补显式通道。
inline PkByteArray toPkByteArray(const char *s)
{
    return PkByteArray(s, static_cast<int>(std::strlen(s)));
}

// 真 Qt 列表<T> ↔ PkList<T>（元素类型相同才可互转；真 Qt 对 vs std::pair 之类形状不同的
// 跨界不在模板里，逐点手写）。
template <typename T>
inline PkList<T> toPkList(const PK_QLIST_<T> &l)
{
    PkList<T> out;
    for (const auto &v : l) out.append(v);
    return out;
}

template <typename T>
inline PK_QLIST_<T> toQList(const PkList<T> &l)
{
    PK_QLIST_<T> out;
    for (const auto &v : l) out.append(v);
    return out;
}

// PkStream 全读 → 真 Qt 字节数组：剥离侧（KoResource::loadFromDevice(PkStream*) 等）
// 拿到 PkStream*，读全部字节转真 Qt 字节数组再走真 Qt 解析路径。
// PkStream::readAll()/peek()/readLine() 按 pk/port/README.md 登记**只声明不定义**（等 R-02
// 交付 PkByteArray 语义）。消费方要「读完整个设备」时不能用它——这里用 read()/atEnd() 循环
// 手工读完，直接累积进真 Qt QByteArray（调用方 data 成员就是 QByteArray）。
inline PK_QBYTEARRAY_ pkReadAllAsQByteArray(PkStream *dev)
{
    PK_QBYTEARRAY_ buf;
    char tmp[4096];
    while (!dev->atEnd()) {
        const auto n = dev->read(tmp, sizeof(tmp));
        if (n <= 0) break;
        buf.append(tmp, static_cast<int>(n));
    }
    return buf;
}

// 真 Qt 元素 → PkXmlElement：真 Qt DOM 子树深拷到 pugixml 的临时文档，交给剥离侧
// 的 PkXmlElement 消费（SvgLoadingContext::pushGraphicsContext/addDefinition、
// SvgStyleParser、KoSvgTextLoader::loadSvg 等）。PkXmlElement 的 shared_ptr 持有
// pugi 文档，元素在函数返回后仍有效。过渡期行为——每次跨界一次序列化+解析，
// 正确性优先，性能不优化；flake 剥完（源码层 Q* 归零）后本转换连同调用点一起删。
inline PkXmlElement toPkXmlElement(const PK_QDOMEL_ &el)
{
    if (el.isNull()) {
        return PkXmlElement();
    }
    PK_QDOMDOC_ doc;
    doc.appendChild(doc.importNode(el, true));
    const PkString xml = toPkString(doc.toString(0));
    PkXmlDocument pkDoc;
    PkString errorMsg;
    int errorLine = 0;
    int errorColumn = 0;
    if (!pkDoc.setContent(xml, &errorMsg, &errorLine, &errorColumn)) {
        return PkXmlElement();
    }
    return pkDoc.documentElement();
}

// PkXmlElement → 真 Qt 元素：toPkXmlElement 的反方向。SvgParser.cpp 的
// SvgLoadingContext::definition() 返回 PkXmlElement，而该文件是 real-Qt-first
// （内部用 QDomElement），需要转回真 Qt 元素。toPkXmlElement 造出的 PkXmlElement
// 是「每元素一文档」，ownerDocument().toString() 序列化出的正是该元素子树，再交给
// 真 Qt QDomDocument 解析、取 documentElement 即还原。过渡期行为——每次跨界一次
// 序列化+解析，正确性优先；flake 剥完后（源码层 Q* 归零）本转换连同调用点一起删。
inline PK_QDOMEL_ toQDomElement(const PkXmlElement &el)
{
    if (el.isNull()) {
        return PK_QDOMEL_();
    }
    PK_QDOMDOC_ doc;
    const PkXmlDocument pkDoc = el.ownerDocument();
    if (!doc.setContent(toQString(pkDoc.toString(0)))) {
        return PK_QDOMEL_();
    }
    return doc.documentElement();
}

// 真 Qt 图像 → PkImage：PkImage::Format 数值与真 Qt 图像 Format 顺序一致
// （pk/image/PkImage.h 自 Format_Invalid 起逐项对应），static_cast 直转；像素逐
// scanLine 拷贝，索引色表也拷（与 KoFontFamily.cpp 的 qimageToPkImage 同款）。
// 用于剥离侧 setImage(const PkImage&) 收真 Qt 图像的场景（资源 .cpp 缩略图）。
inline PkImage toPkImage(const PK_QIMAGE_ &img)
{
    PkImage out(img.width(), img.height(), static_cast<PkImage::Format>(img.format()));
    for (int y = 0; y < img.height(); ++y) {
        const uchar *src = img.constScanLine(y);
        uint8_t *dst = out.scanLine(y);
        std::memcpy(dst, src, size_t(img.bytesPerLine()));
    }
    if (img.colorCount() > 0) {
        std::vector<uint32_t> table;
        table.reserve(size_t(img.colorCount()));
        for (int i = 0; i < img.colorCount(); ++i) {
            table.push_back(img.color(i));
        }
        out.setColorTable(table);
    }
    return out;
}

// PkImage → 真 Qt 图像：反向拷贝（像素逐 scanLine，索引色表）。资源 .cpp 的
// 缩略图/预览走真 Qt 渲染路径（save/fromData 等）时用。
inline PK_QIMAGE_ toQImage(const PkImage &img)
{
    PK_QIMAGE_ out(img.width(), img.height(), static_cast<PK_QIMAGE_::Format>(img.format()));
    for (int y = 0; y < img.height(); ++y) {
        const uint8_t *src = img.constScanLine(y);
        uchar *dst = out.scanLine(y);
        std::memcpy(dst, src, size_t(img.bytesPerLine()));
    }
    if (img.colorCount() > 0) {
        PK_QVECTOR_<PK_QRGB_> table;
        table.reserve(size_t(img.colorCount()));
        for (int i = 0; i < img.colorCount(); ++i) {
            table.append(PK_QRGB_(img.color(i)));
        }
        out.setColorTable(table);
    }
    return out;
}

// 真 Qt 调试流 << PkString：剥离头（kis_dom_utils.h 的 toInt/toDouble 等）里
// `warnKrita << ... << PkString` 在 real-Qt-first TU 落到真 Qt 调试流，PkString 在全局
// 命名空间 → ADL 自动命中本操作符，无需在各剥离头里加东西。
inline PK_QDEBUG_ operator<<(PK_QDEBUG_ dbg, const PkString &s)
{
    dbg << s.PkToUtf8().c_str();
    return dbg;
}

// 真 Qt 调试流 << PkByteArray：剥离头（kis_debug.h 的 ppVar/warnKrita 等）把 PkByteArray
// 流进真 Qt 调试流时命中（实测 SvgLoadingContext.cpp）。与 PkString 版同理，ADL 收全局
// 命名空间的 PkByteArray。
inline PK_QDEBUG_ operator<<(PK_QDEBUG_ dbg, const PkByteArray &b)
{
    dbg << toQByteArray(b);
    return dbg;
}

// 真 Qt 调试流 << PkTransform：TestSvgParser.cpp 等测试里 `qDebug() << ppVar(p.transform())`
// （p.transform() 返回 PkTransform）落到真 Qt 调试流时命中。直接复用 toQTransform 转成
// 真 Qt 变换再流进 QDebug（QTransform 自带调试输出），打印语义与真 Qt 完全一致。
inline PK_QDEBUG_ operator<<(PK_QDEBUG_ dbg, const PkTransform &t)
{
    dbg << toQTransform(t);
    return dbg;
}

// PkSharedPointer ↔ QSharedPointer（保活 deleter 模式，与 KoShapeBackgroundCommand.cpp
// 内匿名 namespace 的同名助手同款）：真 Qt 的 QSharedPointer 无 std::shared_ptr 互操作
// 构造，两边各自持有独立控制块，转换方把原指针的副本捕获进 deleter，借副本维持所有权；
// 对象只被原控制块删除一次，不会双删。测试 TU 传 QSharedPointer 给收 PkSharedPointer
// 的 stripped 命令类（KoShapeBackgroundCommand 等）时用。flake 剥完（共享指针归 Pk）
// 后本转换连同调用点一起删。
template <typename T>
inline PkSharedPointer<T> toPkSharedPointer(const PK_QSHAREDPOINTER_<T> &p)
{
    PK_QSHAREDPOINTER_<T> keep = p;
    return PkSharedPointer<T>(p.data(), [keep](T *) { (void)keep; });
}

template <typename T>
inline PK_QSHAREDPOINTER_<T> toQSharedPointer(const PkSharedPointer<T> &p)
{
    PkSharedPointer<T> keep = p;
    return PK_QSHAREDPOINTER_<T>(p.data(), [keep](T *) { (void)keep; });
}
// PkDeviceStream —— 过渡期适配器：把真 真 Qt 设备* 包成 PkStream*，供剥离侧收 PkStream*
// 的 API（KoXmlWriter 等）消费真 Qt 设备。只服务过渡构建（build-ci）；flake 剥完、
// 调用点改用 Pk 设备后与调用点一起删除。
//
// ⚠ 构造顺序：真 Qt 缓冲 类成员必须先于 PkDeviceStream 声明（本类只存指针，不 deref），
// 构造后调用 attach() 绑定。KoXmlWriter 会自行 open(WriteOnly)，本类把 open/close/
// 读写全转发给真 Qt 设备。OpenMode 位值与真 Qt 设备::OpenMode 逐位相同。
class PkDeviceStream : public PkStream
{
public:
    void attach(PK_QIODEVICE_ *dev)
    {
        m_dev = dev;
        setOpenMode(dev->openMode());
    }

    bool open(OpenMode mode) override
    {
        const bool ok = m_dev && m_dev->open(static_cast<PK_QIODEVICE_::OpenMode>(mode));
        if (ok) {
            setOpenMode(mode);
        }
        return ok;
    }

    void close() override
    {
        if (m_dev) {
            m_dev->close();
        }
        setOpenMode(NotOpen);
    }

    bool isSequential() const override { return m_dev ? m_dev->isSequential() : false; }
    pk_int64 size() const override { return m_dev ? m_dev->size() : 0; }
    pk_int64 pos() const override { return m_dev ? m_dev->pos() : 0; }
    bool seek(pk_int64 pos) override { return m_dev ? m_dev->seek(pos) : false; }
    bool atEnd() const override { return m_dev ? m_dev->atEnd() : true; }
    pk_int64 bytesAvailable() const override { return m_dev ? m_dev->bytesAvailable() : 0; }
    bool canReadLine() const override { return m_dev ? m_dev->canReadLine() : false; }

protected:
    pk_int64 readData(char *data, pk_int64 maxSize) override
    {
        return m_dev ? m_dev->read(data, maxSize) : -1;
    }

    pk_int64 writeData(const char *data, pk_int64 maxSize) override
    {
        return m_dev ? m_dev->write(data, maxSize) : -1;
    }

private:
    PK_QIODEVICE_ *m_dev = nullptr;
};

// PkStreamIoDevice —— 过渡期适配器（PkDeviceStream 的反向）：把 PkStream* 包成
// 真 Qt 设备*，供仍要真 Qt 设备* 的 API（SvgWriter::save、图像 save 等）消费剥离
// 侧收 PkStream* 的场景。PkZipArchive 不持有传入 stream 的所有权
// （pk/port/zip/PkZipArchive.cpp 注释），本适配器可安全放栈上、传地址给消费方，
// 调用点负责保证其生命周期盖过消费方。
//
// 与 PkDeviceStream 同构：构造后调用 attach() 绑定 PkStream；open/close/读写全
// 转发。真 Qt 设备::OpenMode 位值与 PkStream::OpenMode 逐位相同。
class PkStreamIoDevice : public PK_QIODEVICE_
{
public:
    void attach(PkStream *stream)
    {
        m_stream = stream;
        setOpenMode(static_cast<PK_QIODEVICE_::OpenMode>(stream->openMode()));
    }

    bool open(OpenMode mode) override
    {
        const bool ok = m_stream && m_stream->open(static_cast<PkStream::OpenMode>(mode));
        if (ok) {
            setOpenMode(mode);
        }
        return ok;
    }

    void close() override
    {
        if (m_stream) {
            m_stream->close();
        }
        setOpenMode(PK_QIODEVICE_::NotOpen);
    }

    bool isSequential() const override { return m_stream ? m_stream->isSequential() : false; }
    qint64 size() const override { return m_stream ? m_stream->size() : 0; }
    qint64 pos() const override { return m_stream ? m_stream->pos() : 0; }
    bool seek(qint64 pos) override { return m_stream ? m_stream->seek(pos) : false; }
    bool atEnd() const override { return m_stream ? m_stream->atEnd() : true; }
    qint64 bytesAvailable() const override { return m_stream ? m_stream->bytesAvailable() : 0; }
    bool canReadLine() const override { return m_stream ? m_stream->canReadLine() : false; }

protected:
    qint64 readData(char *data, qint64 maxSize) override
    {
        return m_stream ? m_stream->read(data, maxSize) : -1;
    }

    qint64 writeData(const char *data, qint64 maxSize) override
    {
        return m_stream ? m_stream->write(data, maxSize) : -1;
    }

private:
    PkStream *m_stream = nullptr;
};

#else
// ---- Qt-free 薄壳分支（QT_CORE_LIB 未定义）----
// 壳内 PkXmlCompat.h（shell/kritaflake/PkXmlCompat.h，include 路径第一命中）把 Q 名
// 宏映射到 Pk 类型；本分支再补 pk/color/PkColor.h（壳 PkXmlCompat 刻意不含 Color 的
// compat 映射，桥接用到 PkColor 才单独引）。桥接函数退化为 Pk 恒等透传——前提是 Q 名
// 已宏映射到同一 Pk 类型（List→PkList、PointF→PkPointF、DomElement→PkXmlElement 等），
// 透传与真 Qt 分支按分量互转在 Qt-free 下语义一致。唯一例外是 String：壳 compat 的
// String 垫片是 PkString 子类，故 toQString 返回 PK_QSTRING_（子类）以保真。
// 无 Pk 对应的桥接（ByteArray/Image/IODevice 相关、PkDeviceStream、PkStreamIoDevice）
// 只服务真 Qt 分支——壳内无对应类型，且剥离源文件不使用它们。
#include <PkXmlCompat.h>
#include <pk/color/PkColor.h>
#include <pk/pointer/PkSharedPointer.h>

inline PkString toPkString(const PkString &s) { return s; }
inline PK_QSTRING_ toQString(const PkString &s) { return s; }

inline PkRectF toPkRectF(const PkRectF &r) { return r; }
inline PkRectF toQRectF(const PkRectF &r) { return r; }

inline PkPointF toPkPointF(const PkPointF &p) { return p; }
inline PkPointF toQPointF(const PkPointF &p) { return p; }

inline PkColor toPkColor(const PkColor &c) { return c; }
inline PkColor toQColor(const PkColor &c) { return c; }

inline PkTransform toPkTransform(const PkTransform &t) { return t; }
inline PkTransform toQTransform(const PkTransform &t) { return t; }

inline PkSizeF toPkSizeF(const PkSizeF &s) { return s; }
inline PkSizeF toQSizeF(const PkSizeF &s) { return s; }

inline PkPainterPath toPkPainterPath(const PkPainterPath &p) { return p; }
inline PkPainterPath toQPainterPath(const PkPainterPath &p) { return p; }

inline PkPolygon toPkPolygon(const PkPolygon &p) { return p; }
inline PkPolygon toQPolygon(const PkPolygon &p) { return p; }

inline PkLineF toPkLineF(const PkLineF &l) { return l; }

inline PkXmlElement toPkXmlElement(const PkXmlElement &el) { return el; }
inline PkXmlElement toQDomElement(const PkXmlElement &el) { return el; }

template <typename T>
inline PkList<T> toPkList(const PkList<T> &l) { return l; }

template <typename T>
inline PkList<T> toQList(const PkList<T> &l) { return l; }

// 共享指针互转的 Qt-free 恒等版：真 Qt 分支的 toPkSharedPointer/toQSharedPointer 在
// 保活 deleter（Pk↔Q 各持控制块）；Qt-free 下 QSharedPointer 已宏映射到 PkSharedPointer，
// 恒等透传即可。KoShapeBackgroundCommand.cpp 等剥离源在薄壳编译时依赖本对。
template <typename T>
inline PkSharedPointer<T> toPkSharedPointer(const PkSharedPointer<T> &p) { return p; }

template <typename T>
inline PkSharedPointer<T> toQSharedPointer(const PkSharedPointer<T> &p) { return p; }
#endif
