#include "PkXmlDocument.h"

#include <sstream>
#include <string>

PkXmlDocument::PkXmlDocument()
    : PkXmlNode()
{
    _doc = std::make_shared<pugi::xml_document>();
    _node = *_doc;
    _kind = Kind::Node;
}

PkXmlDocument::PkXmlDocument(const PkString &docName)
    : PkXmlDocument()
{
    // Qt: QDomDocument(const QString &name) 创建文档的同时设好 doctype 的
    // name（publicId/systemId 留空）——真实调用点没有用到这个构造函数带出的
    // doctype 的 publicId/systemId（用量表没出现），只存 name 即可对齐。
    if (!docName.isEmpty()) {
        pugi::xml_node dt = _doc->append_child(pugi::node_doctype);
        dt.set_value(docName.PkToUtf8().c_str());
    }
}

pugi::xml_node PkXmlDocument::ensureLimboNode()
{
    for (pugi::xml_node c = _doc->first_child(); c; c = c.next_sibling()) {
        if (pkIsXmlLimboNode(c)) {
            return c;
        }
    }
    pugi::xml_node limbo = _doc->append_child(pugi::node_element);
    limbo.set_name(kPkXmlLimboTag);
    return limbo;
}

PkXmlElement PkXmlDocument::createElement(const PkString &tagName)
{
    // 创建即挂树：见 PkXmlNode.h 顶部注释——新节点立刻挂进 limbo 容器（而不是
    // 真正悬空的，也不是文档根的直接子节点），appendChild() 用 append_move()
    // 在同一棵树内部把它从 limbo 搬到真正的目标位置。
    pugi::xml_node n = ensureLimboNode().append_child(pugi::node_element);
    n.set_name(tagName.PkToUtf8().c_str());
    return PkXmlElement(n, _doc);
}

PkXmlText PkXmlDocument::createTextNode(const PkString &value)
{
    pugi::xml_node n = ensureLimboNode().append_child(pugi::node_pcdata);
    n.set_value(value.PkToUtf8().c_str());
    return PkXmlText(n, _doc);
}

PkXmlCDATASection PkXmlDocument::createCDATASection(const PkString &value)
{
    pugi::xml_node n = ensureLimboNode().append_child(pugi::node_cdata);
    n.set_value(value.PkToUtf8().c_str());
    return PkXmlCDATASection(n, _doc);
}

PkXmlElement PkXmlDocument::documentElement() const
{
    for (pugi::xml_node c = _doc->first_child(); c; c = c.next_sibling()) {
        // limbo 容器本身也是 node_element 类型，必须先排除，否则孤儿节点
        // 堆积多了之后 limbo 会被误当成"第一个元素子节点"返回——见 R-07 全
        // 分支终审 I1，PkXmlNode.h 顶部类注释。
        if (c.type() == pugi::node_element && !pkIsXmlLimboNode(c)) {
            return PkXmlElement(c, _doc);
        }
    }
    return PkXmlElement();
}

bool PkXmlDocument::setContent(const PkString &xml, PkString *errorMsg, int *errorLine,
                                int *errorColumn)
{
    return setContent(xml, false, errorMsg, errorLine, errorColumn);
}

bool PkXmlDocument::setContent(const PkString &xml, bool namespaceProcessing, PkString *errorMsg,
                                int *errorLine, int *errorColumn)
{
    // pugixml 本身不做命名空间解析（探针 P4 之后的关键实现说明 5）——两个重载
    // 走同一套 parse_default 解析流程，namespaceProcessing 参数只是为了签名
    // 对齐 Qt；命名空间处理在 PkXmlElement::localName()/namespaceURI() 里实现。
    (void)namespaceProcessing;
    return setContentImpl(xml, pugi::parse_default | pugi::parse_doctype, errorMsg, errorLine,
                           errorColumn);
}

bool PkXmlDocument::setContentPreservingWhitespace(const PkString &xml, bool namespaceProcessing,
                                                    PkString *errorMsg, int *errorLine,
                                                    int *errorColumn)
{
    // I2（R-07 全分支终审）：额外加 parse_ws_pcdata——普通 setContent()（P1
    // 探针对齐 Qt 丢弃纯空白文本节点）不适用于"空白有语义"的真实调用点
    // （SvgParser.cpp，见 README 已知偏离清单 I2）。
    (void)namespaceProcessing;
    return setContentImpl(xml, pugi::parse_default | pugi::parse_doctype | pugi::parse_ws_pcdata,
                           errorMsg, errorLine, errorColumn);
}

bool PkXmlDocument::setContentImpl(const PkString &xml, unsigned int parseFlags,
                                    PkString *errorMsg, int *errorLine, int *errorColumn)
{
    const std::string utf8 = xml.PkToUtf8();
    // 重复调用 setContent 要能覆盖上一次的树（Qt 语义），reset() 之后同一个
    // shared_ptr<pugi::xml_document> 重新 load——旧的借出句柄语义上失效，与
    // Qt 的"整棵树被替换"效果一致。这也会连带清掉任何还留在 limbo 里、尚未
    // appendChild 的孤儿节点——与 Qt 语义一致（没有等价概念，reset 即可）。
    _doc->reset();
    // parse_default 不含 parse_doctype（pugixml 默认丢弃 DOCTYPE 声明，见
    // src/pugixml.hpp parse_full 的定义）——补上它，否则 doctype() 永远找不到
    // 任何从 setContent 解析出来的 DOCTYPE 节点。已现场探针确认：`<!DOCTYPE
    // note SYSTEM "Note.dtd">` 解析后 node_doctype 的 value() 是整段原始文本
    // `note SYSTEM "Note.dtd"`，doctype() 取第一个空白分隔 token 当 name()。
    const pugi::xml_parse_result result =
        _doc->load_buffer(utf8.data(), utf8.size(), parseFlags);
    _node = *_doc;

    if (result) {
        return true;
    }

    if (errorMsg) {
        *errorMsg = pkXmlFromPugi(result.description());
    }
    if (errorLine || errorColumn) {
        // 探针 P4：Qt 的 errLine/errCol 是 1-based。pugixml 只给字节 offset，
        // 没有现成的行列号——扫一遍已解析前缀自己数换行/列。
        int line = 1;
        int col = 1;
        for (std::ptrdiff_t i = 0;
             i < result.offset && i < static_cast<std::ptrdiff_t>(utf8.size()); ++i) {
            if (utf8[static_cast<std::size_t>(i)] == '\n') {
                ++line;
                col = 1;
            } else {
                ++col;
            }
        }
        if (errorLine) {
            *errorLine = line;
        }
        if (errorColumn) {
            *errorColumn = col;
        }
    }
    return false;
}

PkXmlNode PkXmlDocument::importNode(const PkXmlNode &importedNode, bool deep)
{
    // 探针 P13：传入 null 节点返回 null。
    if (importedNode.isNull()) {
        return PkXmlNode();
    }

    const pugi::xml_node src = pkRawNode(importedNode);

    if (deep) {
        // pugixml 的 append_copy() 本来就是深拷贝（含全部子树 + 属性），
        // 天然覆盖这个分支——探针 P13 确认的 Qt 语义正是"深拷贝一份改了
        // 身份的新副本"，跟 append_copy() 是同一件事，不需要额外代码。
        pugi::xml_node n = ensureLimboNode().append_copy(src);
        return PkXmlNode(n, _doc);
    }

    // deep == false：手工建同类型同名节点，只拷贝属性，不递归拷贝子节点。
    pugi::xml_node n = ensureLimboNode().append_child(src.type());
    n.set_name(src.name());
    // set_value() 对 node_element 是无操作（pugixml 只对 pcdata/cdata/
    // comment/pi/doctype 类型生效），对 node_pcdata/node_cdata 才有意义
    // ——两种情形都安全调用，不需要按类型分支。
    n.set_value(src.value());
    for (pugi::xml_attribute a = src.first_attribute(); a; a = a.next_attribute()) {
        pugi::xml_attribute newAttr = n.append_attribute(a.name());
        newAttr.set_value(a.value());
    }
    return PkXmlNode(n, _doc);
}

PkString PkXmlDocument::toString(int indent) const
{
    // 探针 P3：indent >= 0 时结尾带一个 '\n'（format_indent 天然产出）；
    // indent < 0（含默认值 1 判定用的是 `< 0`，不是 `== -1`——但默认值 1 走的
    // 是缩进分支）走单行紧凑模式（format_raw）且无结尾换行。
    // format_no_declaration：Qt 的 toString() 不输出 `<?xml ?>` 声明（即便
    // setContent 解析到了声明，Qt 也不保留/不回显它）。
    unsigned int flags = pugi::format_no_declaration;
    std::string indentStr;
    if (indent < 0) {
        flags |= pugi::format_raw;
    } else {
        flags |= pugi::format_indent;
        indentStr.assign(static_cast<std::size_t>(indent), ' ');
    }

    // limbo 容器（R-07 全分支终审 I1）里挂着的孤儿节点不是真实文档结构的
    // 一部分，不能出现在序列化输出里——pugixml 没有"临时隐藏子节点再复原"的
    // API（remove_child 会真正释放内存，不可逆），深拷贝一份跳过 limbo 的
    // 临时文档再 save() 是最简单可靠的做法：只多一次 O(n) 深拷贝，换来不用
    // 碰活树的结构。
    pugi::xml_document tmp;
    for (pugi::xml_node c = _doc->first_child(); c; c = c.next_sibling()) {
        if (pkIsXmlLimboNode(c)) {
            continue;
        }
        tmp.append_copy(c);
    }

    std::ostringstream oss;
    tmp.save(oss, indentStr.c_str(), flags, pugi::encoding_utf8);
    const std::string s = oss.str();
    return PkString::PkFromUtf8(s.c_str(), static_cast<int>(s.size()));
}

PkString PkXmlDocument::toByteArray(int indent) const
{
    // 已知偏离，见头文件类注释：PkByteArray 尚未交付，退化成 UTF-8 编码的
    // PkString，行为等价于 toString()。
    return toString(indent);
}

PkXmlDocumentType PkXmlDocument::doctype() const
{
    for (pugi::xml_node c = _doc->first_child(); c; c = c.next_sibling()) {
        if (c.type() == pugi::node_doctype) {
            const std::string raw = c.value();
            const std::size_t sp = raw.find_first_of(" \t\r\n");
            const std::string name = (sp == std::string::npos) ? raw : raw.substr(0, sp);
            return PkXmlDocumentType(c, _doc,
                                      PkString::PkFromUtf8(name.c_str(),
                                                            static_cast<int>(name.size())));
        }
    }
    return PkXmlDocumentType();
}
