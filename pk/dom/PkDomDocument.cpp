#include "PkDomDocument.h"
#include <pugixml.hpp>

// 把 pugixml 的节点树深拷贝成 PkDom 树。document 拥有整棵树（见 PkDom.h 析构）。
namespace {
PkDomNode *buildTree(PkDomDocument *doc, const pugi::xml_node &n) {
    PkDomElement *e = doc->createElement(n.name());
    for (pugi::xml_attribute attr = n.first_attribute(); attr; attr = attr.next_attribute())
        e->setAttribute(attr.name(), attr.value());
    for (pugi::xml_node child = n.first_child(); child; child = child.next_sibling()) {
        pugi::xml_node_type t = child.type();
        if (t == pugi::node_element) {
            e->appendChild(buildTree(doc, child));
        } else if (t == pugi::node_pcdata || t == pugi::node_cdata) {
            e->appendChild(doc->createTextNode(child.value()));
        }
        // 注释 / 处理指令 / DOCTYPE / 声明：QDom 默认不计，跳过
    }
    return e;
}
} // namespace

bool PkDomDocument::setContent(const std::string &data, bool /*namespaceProcessing*/,
                               std::string *errorMsg, int *errorLine, int *errorCol) {
    pugi::xml_document pdoc;
    // parse_default 与 QDomDocument 默认一致：丢弃纯空白文本节点、不解析命名空间。
    pugi::xml_parse_result res = pdoc.load_string(data.c_str());
    if (!res) {
        if (errorMsg) *errorMsg = res.description();
        if (errorLine) *errorLine = static_cast<int>(res.offset);  // 字节偏移近似
        if (errorCol) *errorCol = -1;
        return false;
    }
    pugi::xml_node root = pdoc.document_element();
    if (!root) {
        if (errorMsg) *errorMsg = "no root element";
        return false;
    }
    PkDomElement *el = static_cast<PkDomElement *>(buildTree(this, root));
    appendChild(el);   // el 的 parent = 本 document，document 析构时整体释放
    m_root = el;
    return true;
}
