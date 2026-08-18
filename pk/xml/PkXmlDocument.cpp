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

PkXmlElement PkXmlDocument::createElement(const PkString &tagName)
{
    // 创建即挂树：见 PkXmlNode.h 顶部注释——新节点立刻成为文档根的子节点，
    // 而不是真正悬空的，appendChild() 用 append_move() 在同一棵树内部搬走它。
    pugi::xml_node n = _doc->append_child(pugi::node_element);
    n.set_name(tagName.PkToUtf8().c_str());
    return PkXmlElement(n, _doc);
}

PkXmlText PkXmlDocument::createTextNode(const PkString &value)
{
    pugi::xml_node n = _doc->append_child(pugi::node_pcdata);
    n.set_value(value.PkToUtf8().c_str());
    return PkXmlText(n, _doc);
}

PkXmlCDATASection PkXmlDocument::createCDATASection(const PkString &value)
{
    pugi::xml_node n = _doc->append_child(pugi::node_cdata);
    n.set_value(value.PkToUtf8().c_str());
    return PkXmlCDATASection(n, _doc);
}

PkXmlElement PkXmlDocument::documentElement() const
{
    for (pugi::xml_node c = _doc->first_child(); c; c = c.next_sibling()) {
        if (c.type() == pugi::node_element) {
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

    const std::string utf8 = xml.PkToUtf8();
    // 重复调用 setContent 要能覆盖上一次的树（Qt 语义），reset() 之后同一个
    // shared_ptr<pugi::xml_document> 重新 load——旧的借出句柄语义上失效，与
    // Qt 的"整棵树被替换"效果一致。
    _doc->reset();
    // parse_default 不含 parse_doctype（pugixml 默认丢弃 DOCTYPE 声明，见
    // src/pugixml.hpp parse_full 的定义）——补上它，否则 doctype() 永远找不到
    // 任何从 setContent 解析出来的 DOCTYPE 节点。已现场探针确认：`<!DOCTYPE
    // note SYSTEM "Note.dtd">` 解析后 node_doctype 的 value() 是整段原始文本
    // `note SYSTEM "Note.dtd"`，doctype() 取第一个空白分隔 token 当 name()。
    const pugi::xml_parse_result result = _doc->load_buffer(
        utf8.data(), utf8.size(), pugi::parse_default | pugi::parse_doctype);
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

    std::ostringstream oss;
    _doc->save(oss, indentStr.c_str(), flags, pugi::encoding_utf8);
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
