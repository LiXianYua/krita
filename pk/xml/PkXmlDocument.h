#pragma once

#include <cstddef>

#include "PkXmlCDATASection.h"
#include "PkXmlElement.h"
#include "PkXmlImplementation.h"
#include "PkXmlNode.h"
#include "PkXmlText.h"

// R-25 Task 2：三个新增 setContent 重载家族的形参类型，全部只用指针/引用，
// 头文件不需要它们的完整定义——前置声明即可，避免把 pk/port（PkStream）的
// include 链无条件拖进这个已经很小的公开头文件。
class PkStream;        // pk/port/PkStream.h（R-12 交付），QIODevice 的零 Qt 对应物。
class PkXmlStreamReader; // 本目录自己交付的类型，见 PkXmlStreamReader.h。
// PkByteArray 归 R-02，尚未交付——见下方 setContent(const PkByteArray&) 声明处的说明。
class PkByteArray;

// PkXmlDocument —— QDomDocument 的零 Qt 对应物：整棵 pugi::xml_document 树的根，
// 唯一拥有 `_doc`（shared_ptr<pugi::xml_document>）内存的地方（其它 PkXmlNode/
// PkXmlElement 只是从这里借出的句柄，靠共享同一个 shared_ptr 延续树的生命期）。
//
// `toByteArray()` 的已知偏离：brief 要求的签名是 `PkByteArray toByteArray(...)`，
// 但本 worktree 当前没有交付 `PkByteArray`（R-02 尚未产出，`pk/container/` 下
// 没有任何 *ByteArray* 文件——已现场确认）。按 brief 关键实现说明 5 的退化
// 路径：改用 `PkString`（UTF-8 编码内容）代替，行为等价于 `toString()`。
// 真实调用点 23 处用到 `toByteArray`，其中相当一部分紧跟着 `.data()`/
// 隐式转 `const char*` 之类只依赖字节内容的用法——PkString 的
// `PkToUtf8()`/`PkFromUtf8()` 互操作能覆盖，具体收窄留给试接任务（Task 3/4）
// 实测确认。若后续 R-02 交付 PkByteArray，这里要改签名，是已知技术债，
// 不是本任务遗漏。
class PkXmlDocument : public PkXmlNode
{
public:
    PkXmlDocument();
    explicit PkXmlDocument(const PkString &docName);
    // 内部构造：供 PkXmlNode::ownerDocument() 等跨类构造使用，不是 Qt 兼容
    // 表面的一部分——它复用调用方已有的 _doc，不新建树。
    PkXmlDocument(pugi::xml_node node, std::shared_ptr<pugi::xml_document> doc)
        : PkXmlNode(node, std::move(doc))
    {
    }

    PkXmlElement createElement(const PkString &tagName);
    PkXmlText createTextNode(const PkString &value);
    PkXmlCDATASection createCDATASection(const PkString &value);

    PkXmlElement documentElement() const;

    bool setContent(const PkString &xml, PkString *errorMsg = nullptr, int *errorLine = nullptr,
                     int *errorColumn = nullptr);
    bool setContent(const PkString &xml, bool namespaceProcessing, PkString *errorMsg = nullptr,
                     int *errorLine = nullptr, int *errorColumn = nullptr);

    // R-25 Task 2（①-a）：QDomDocument::setContent(QIODevice*, ...) 两个重载的
    // 零 Qt 对应物。探针 P14（$PK/docs/superpowers/plans/R-25.md）确认：从
    // `device` 的**当前位置**开始读到 EOF（不自己 seek(0)），EOF 不是错误。
    // 实现循环调用 `device->read()` 拼字节，直接喂给字节导向的
    // `setContentImpl(const char*, ...)`——不经过 PkString 中转，保留 pugixml
    // 自己的编码自动探测（P14 探针的"尊重 XML 声明 encoding"结论）。
    bool setContent(PkStream *device, PkString *errorMsg = nullptr, int *errorLine = nullptr,
                     int *errorColumn = nullptr);
    bool setContent(PkStream *device, bool namespaceProcessing, PkString *errorMsg = nullptr,
                     int *errorLine = nullptr, int *errorColumn = nullptr);

    // R-25 Task 2（①-b）：QDomDocument::setContent(QXmlStreamReader*, bool, ...)
    // 的零 Qt 对应物——把 PkXmlStreamReader 当 StAX token 源，驱动一个
    // "流转 DOM" 构建器（循环 readNext()，按 tokenType() 建节点/挂树/游标下探
    // 上浮），跟 SvgParser.cpp:201 的调用形状对齐。这是有意打破 Task 1 定下的
    // "DOM/Stream 两侧不互相消费对方类型"边界——见 pk/xml/README.md §11.3 与
    // 本类 `.cpp` 里这个重载的实现注释。
    bool setContent(PkXmlStreamReader *reader, bool namespaceProcessing,
                     PkString *errorMsg = nullptr, int *errorLine = nullptr,
                     int *errorColumn = nullptr);

    // R-25 Task 2（①-c）：QDomDocument::setContent(QByteArray, ...) 的形状占位
    // ——只声明，不定义。`QByteArray` 是真 Qt 类型，本 worktree 没有交付
    // `PkByteArray`（R-02）也没有 `pk/container/` 下的 QByteArray compat
    // 映射，真实调用点 `KoColorSet.cpp:1918` 无法在本任务 `locks`（只有
    // `pk/xml`）范围内零改动试接——不是设计选择，是范围边界。照抄
    // `pk/port/PkStream.h` 自己已通过评审的先例（`readAll()`/`peek()`/
    // `readLine()` 三个返回 `PkByteArray` 的方法同样只声明不定义）：链接期报
    // `undefined reference` 是预期行为，不要为了让它"能跑"造一个假实现。
    bool setContent(const PkByteArray &data, PkString *errorMsg = nullptr,
                     int *errorLine = nullptr, int *errorColumn = nullptr);

    PkString toString(int indent = 1) const;
    PkString toByteArray(int indent = 1) const; // 已知偏离，见类注释

    PkXmlDocumentType doctype() const;

    // R-25 Task 1：QDomDocument::importNode(const QDomNode &, bool deep) 的零
    // Qt 对应物——探针 P13（$PK/docs/superpowers/plans/R-25.md）确认这是
    // **深拷贝**，源节点不受影响（即便同文档内调用也是拷贝，不是移动）；
    // `deep=false` 只拷贝节点自身 + 属性，不递归子节点；传入 null 节点返回
    // null；返回值不挂在文档结构里，跟 createElement() 一样走 limbo 容器，
    // 调用方要自己 appendChild() 才挂树。见 PkXmlNode.h 顶部类注释"创建即
    // 挂树"一节——importNode() 复用同一套 limbo 机制，不是新设计。
    PkXmlNode importNode(const PkXmlNode &importedNode, bool deep);

    // I2（R-07 全分支终审）新增：保留纯空白 PCDATA 节点的 setContent 变体，
    // 对应 pugixml 的 parse_ws_pcdata 标志——普通 setContent()（P1 探针对齐）
    // 会像 Qt 的 QDomDocument 一样丢弃纯空白文本节点，但真实调用点
    // `libs/flake/svg/SvgParser.cpp:201` 用的是
    // `QDomDocument::ParseOption::PreserveSpacingOnlyNodes`（新式 API 分支）/
    // 老式 `setContent(QXmlStreamReader*, false, ...)` 分支下 SVG 文本解析
    // 依赖空白节点被保留（见 README「已知偏离清单」I2）。这是 Qt 没有的
    // pk 专属扩展方法名（不是隐藏的第三个布尔参数——那样会和上面两个重载在
    // 调用点上难以区分意图），不是 Qt 兼容表面本身，供后续任务移植
    // SvgParser.cpp 时使用。
    bool setContentPreservingWhitespace(const PkString &xml, bool namespaceProcessing = false,
                                         PkString *errorMsg = nullptr, int *errorLine = nullptr,
                                         int *errorColumn = nullptr);

private:
    // limbo 容器（R-07 全分支终审 I1）：找到（或懒创建）文档根下那个隐藏的
    // limbo 容器子节点——createElement()/createTextNode()/createCDATASection()
    // 用它暂存"已创建但未 appendChild"的孤儿节点。见 PkXmlNode.h 顶部类注释
    // 与 pkIsXmlLimboNode()。
    pugi::xml_node ensureLimboNode();

    // setContent()/setContentPreservingWhitespace() 共用的解析核心，
    // parseFlags 由调用方按需要加 pugi::parse_ws_pcdata。改造成先转 UTF-8
    // 字节，再转调下面字节导向的版本（R-25 Task 2）——避免重复代码，行为
    // 不变（已有测试必须继续绿）。
    bool setContentImpl(const PkString &xml, unsigned int parseFlags, PkString *errorMsg,
                         int *errorLine, int *errorColumn);

    // R-25 Task 2（①-a）新增：字节导向版本，直接把原始字节喂给
    // pugi::xml_document::load_buffer，不经过 PkString 中转——探针 P14 证实
    // 这样才能保留 pugixml 自己的编码自动探测能力（PkString::PkFromUtf8 是
    // "假定输入已经是 UTF-8"的转换，不做编码探测，会丢掉这个能力）。
    bool setContentImpl(const char *data, std::size_t size, unsigned int parseFlags,
                         PkString *errorMsg, int *errorLine, int *errorColumn);
};
