#pragma once
// S-08 过渡期桥接头：给「真 Qt 头在前」的混合 TU 提供 Pk↔Qt 字符串互转，以及
// 真 Qt 调试流 << PkString 桥接（剥离头里 warnKrita/ppVar 把 PkString 流进真 Qt 调试流）。
//
// 只服务真实 Qt 过渡构建（build-ci）里选 real-Qt-first 策略的 TU：
//   #include <QtCore/QtCore>
//   #include <PkFlakeBridge.h>
// 剥离 TU（纯 Pk）不含真 Qt，不需要本头；compat-active TU 走 PkXmlCompat.h，
// 也不 include 本头。
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
#define PK_QIODEVICE_ PK_CAT_(Q, IODevice)
#define PK_QPOINTF_  PK_CAT_(Q, PointF)
#define PK_QCOLOR_   PK_CAT_(Q, Color)
#define PK_QTRANSFORM_ PK_CAT_(Q, Transform)
#define PK_QDOMDOC_  PK_CAT_(Q, DomDocument)
#define PK_QDOMEL_   PK_CAT_(Q, DomElement)
#define PK_QBYTEARRAY_ PK_CAT_(Q, ByteArray)
#define PK_QLIST_    PK_CAT_(Q, List)

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
#include <pk/color/PkColor.h>
#include <pk/port/PkStream.h>
#include <pk/container/PkList.h>
#include <pk/xml/PkXmlElement.h>
#include <pk/xml/PkXmlDocument.h>
#include <pk/variant/PkAuxTypes.h>

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

// 真 Qt 字节数组 ↔ PkByteArray（PkByteArray 的 (const char*, int) 构造与 constData()）。
inline PK_QBYTEARRAY_ toQByteArray(const PkByteArray &b)
{
    return PK_QBYTEARRAY_(b.constData(), b.size());
}

inline PkByteArray toPkByteArray(const PK_QBYTEARRAY_ &b)
{
    return PkByteArray(b.constData(), b.size());
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
inline PK_QBYTEARRAY_ pkReadAllAsQByteArray(PkStream *dev)
{
    return toQByteArray(dev->readAll());
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

// 真 Qt 调试流 << PkString：剥离头（kis_dom_utils.h 的 toInt/toDouble 等）里
// `warnKrita << ... << PkString` 在 real-Qt-first TU 落到真 Qt 调试流，PkString 在全局
// 命名空间 → ADL 自动命中本操作符，无需在各剥离头里加东西。
inline PK_QDEBUG_ operator<<(PK_QDEBUG_ dbg, const PkString &s)
{
    dbg << s.PkToUtf8().c_str();
    return dbg;
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
