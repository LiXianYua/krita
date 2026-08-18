#pragma once

#include "PkXmlCDATASection.h"
#include "PkXmlElement.h"
#include "PkXmlImplementation.h"
#include "PkXmlNode.h"
#include "PkXmlText.h"

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

    PkString toString(int indent = 1) const;
    PkString toByteArray(int indent = 1) const; // 已知偏离，见类注释

    PkXmlDocumentType doctype() const;

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
    // parseFlags 由调用方按需要加 pugi::parse_ws_pcdata。
    bool setContentImpl(const PkString &xml, unsigned int parseFlags, PkString *errorMsg,
                         int *errorLine, int *errorColumn);
};
