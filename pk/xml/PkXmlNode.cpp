#include "PkXmlNode.h"

#include "PkXmlAttr.h"
#include "PkXmlCDATASection.h"
#include "PkXmlDocument.h"
#include "PkXmlElement.h"
#include "PkXmlNodeList.h"
#include "PkXmlOffsetUtil.h"
#include "PkXmlText.h"

PkXmlNode::PkXmlNode() = default;

PkXmlNode::PkXmlNode(pugi::xml_node node, std::shared_ptr<pugi::xml_document> doc)
    // R-25 Task 3：构造函数签名保持不变（仍接受基类 shared_ptr<pugi::xml_document>
    // ——不改任何既有调用点一个字），内部用 static_pointer_cast 存进
    // shared_ptr<PkXmlDocRoot> 类型的 `_doc` 成员。这个 downcast 是安全的：
    // 传进来的 doc 永远来自 PkXmlDocument（唯一新建/拥有这块内存的地方，
    // 见 PkXmlDocument.cpp），那里已经改成 make_shared<PkXmlDocRoot>()——
    // 每一个流进这个构造函数的 shared_ptr 底层对象都真的是 PkXmlDocRoot。
    : _node(node), _doc(std::static_pointer_cast<PkXmlDocRoot>(std::move(doc))), _kind(Kind::Node)
{
}

PkXmlNode::PkXmlNode(pugi::xml_node ownerNode, pugi::xml_attribute attr,
                      std::shared_ptr<pugi::xml_document> doc)
    : _node(ownerNode), _attr(attr),
      _doc(std::static_pointer_cast<PkXmlDocRoot>(std::move(doc))), _kind(Kind::Attr)
{
}

bool PkXmlNode::isNull() const
{
    return _kind == Kind::Attr ? !_attr : _node.empty();
}

bool PkXmlNode::isElement() const
{
    return _kind == Kind::Node && _node.type() == pugi::node_element;
}

bool PkXmlNode::isText() const
{
    // Qt: isText() 只匹配"真正的" QDomText（nodeType()==TextNode），
    // QDomCDATASection 虽然继承 QDomText，但 nodeType() 是 CDATASectionNode，
    // isText() 对它返回 false——只匹配 pugixml 的 node_pcdata，不含 node_cdata。
    return _kind == Kind::Node && _node.type() == pugi::node_pcdata;
}

bool PkXmlNode::isCDATASection() const
{
    return _kind == Kind::Node && _node.type() == pugi::node_cdata;
}

bool PkXmlNode::isAttr() const
{
    return _kind == Kind::Attr && static_cast<bool>(_attr);
}

PkXmlElement PkXmlNode::toElement() const
{
    if (isElement()) {
        return PkXmlElement(_node, _doc);
    }
    return PkXmlElement();
}

PkXmlText PkXmlNode::toText() const
{
    if (isText()) {
        return PkXmlText(_node, _doc);
    }
    return PkXmlText();
}

PkXmlCDATASection PkXmlNode::toCDATASection() const
{
    if (isCDATASection()) {
        return PkXmlCDATASection(_node, _doc);
    }
    return PkXmlCDATASection();
}

PkXmlAttr PkXmlNode::toAttr() const
{
    if (isAttr()) {
        return PkXmlAttr(_node, _attr, _doc);
    }
    return PkXmlAttr();
}

PkString PkXmlNode::nodeName() const
{
    if (_kind == Kind::Attr) {
        return _attr ? pkXmlFromPugi(_attr.name()) : PkString();
    }
    switch (_node.type()) {
    case pugi::node_element:
        return pkXmlFromPugi(_node.name());
    case pugi::node_pcdata:
        return PkString("#text");
    case pugi::node_cdata:
        return PkString("#cdata-section");
    case pugi::node_document:
        return PkString("#document");
    case pugi::node_comment:
        return PkString("#comment");
    case pugi::node_doctype:
        return PkString("#doctype");
    default:
        return pkXmlFromPugi(_node.name());
    }
}

PkXmlNodeList PkXmlNode::childNodes() const
{
    PkXmlNodeList list;
    if (_kind != Kind::Node) {
        return list;
    }
    for (pugi::xml_node c = _node.first_child(); c; c = c.next_sibling()) {
        // limbo 容器（R-07 全分支终审 I1）不是真实文档结构的一部分，见
        // PkXmlNode.h 顶部类注释——跳过，不出现在任何调用方看到的子节点列表里。
        if (pkIsXmlLimboNode(c)) {
            continue;
        }
        list.PkAppend(PkXmlNode(c, _doc));
    }
    return list;
}

PkXmlNode PkXmlNode::firstChild() const
{
    if (_kind != Kind::Node) {
        return PkXmlNode();
    }
    pugi::xml_node c = _node.first_child();
    while (c && pkIsXmlLimboNode(c)) {
        c = c.next_sibling();
    }
    return PkXmlNode(c, _doc);
}

PkXmlNode PkXmlNode::lastChild() const
{
    if (_kind != Kind::Node) {
        return PkXmlNode();
    }
    pugi::xml_node c = _node.last_child();
    while (c && pkIsXmlLimboNode(c)) {
        c = c.previous_sibling();
    }
    return PkXmlNode(c, _doc);
}

PkXmlNode PkXmlNode::nextSibling() const
{
    if (_kind != Kind::Node) {
        return PkXmlNode();
    }
    pugi::xml_node c = _node.next_sibling();
    while (c && pkIsXmlLimboNode(c)) {
        c = c.next_sibling();
    }
    return PkXmlNode(c, _doc);
}

PkXmlNode PkXmlNode::previousSibling() const
{
    if (_kind != Kind::Node) {
        return PkXmlNode();
    }
    pugi::xml_node c = _node.previous_sibling();
    while (c && pkIsXmlLimboNode(c)) {
        c = c.previous_sibling();
    }
    return PkXmlNode(c, _doc);
}

PkXmlNode PkXmlNode::parentNode() const
{
    if (_kind != Kind::Node) {
        return PkXmlNode();
    }
    pugi::xml_node p = _node.parent();
    if (pkIsXmlLimboNode(p)) {
        // limbo 对调用方透明：孤儿节点的 parentNode() 规整化报告成文档本身，
        // 不暴露 limbo 这个内部实现节点——见 PkXmlNode.h 顶部类注释。
        p = p.parent();
    }
    return PkXmlNode(p, _doc);
}

bool PkXmlNode::hasChildNodes() const
{
    return static_cast<bool>(firstChild()._node);
}

PkXmlNode PkXmlNode::appendChild(const PkXmlNode &newChild)
{
    if (_kind != Kind::Node || newChild._kind != Kind::Node) {
        return PkXmlNode();
    }
    pugi::xml_node moved = _node.append_move(newChild._node);
    if (!moved) {
        return PkXmlNode();
    }
    return PkXmlNode(moved, _doc);
}

PkXmlNode PkXmlNode::insertBefore(const PkXmlNode &newChild, const PkXmlNode &refChild)
{
    if (_kind != Kind::Node || newChild._kind != Kind::Node) {
        return PkXmlNode();
    }
    if (refChild.isNull()) {
        return appendChild(newChild);
    }
    pugi::xml_node moved = _node.insert_move_before(newChild._node, refChild._node);
    if (!moved) {
        return PkXmlNode();
    }
    return PkXmlNode(moved, _doc);
}

bool PkXmlNode::removeChild(const PkXmlNode &oldChild)
{
    if (_kind != Kind::Node || oldChild._kind != Kind::Node) {
        return false;
    }
    return _node.remove_child(oldChild._node);
}

PkXmlDocument PkXmlNode::ownerDocument() const
{
    if (!_doc) {
        return PkXmlDocument();
    }
    return PkXmlDocument(*_doc, _doc);
}

// 四个便捷方法：算法与 PkXmlElement.cpp 里同名方法逐字一致（跳过非元素
// 兄弟/子节点），唯一差别是这里从 `_node` 出发而不要求 `this` 本身是元素——
// 与真 Qt QDomNode 的语义一致（QDomNode 本身可以是任意节点类型，这四个方法
// 只挑子节点/兄弟节点里的元素）。

PkXmlElement PkXmlNode::firstChildElement(const PkString &tagName) const
{
    if (_kind != Kind::Node) {
        return PkXmlElement();
    }
    const std::string want = tagName.PkToUtf8();
    for (pugi::xml_node c = _node.first_child(); c; c = c.next_sibling()) {
        if (c.type() == pugi::node_element && (want.empty() || want == c.name())) {
            return PkXmlElement(c, _doc);
        }
    }
    return PkXmlElement();
}

PkXmlElement PkXmlNode::lastChildElement(const PkString &tagName) const
{
    if (_kind != Kind::Node) {
        return PkXmlElement();
    }
    const std::string want = tagName.PkToUtf8();
    for (pugi::xml_node c = _node.last_child(); c; c = c.previous_sibling()) {
        if (c.type() == pugi::node_element && (want.empty() || want == c.name())) {
            return PkXmlElement(c, _doc);
        }
    }
    return PkXmlElement();
}

PkXmlElement PkXmlNode::nextSiblingElement(const PkString &tagName) const
{
    if (_kind != Kind::Node) {
        return PkXmlElement();
    }
    const std::string want = tagName.PkToUtf8();
    for (pugi::xml_node c = _node.next_sibling(); c; c = c.next_sibling()) {
        if (c.type() == pugi::node_element && (want.empty() || want == c.name())) {
            return PkXmlElement(c, _doc);
        }
    }
    return PkXmlElement();
}

PkXmlElement PkXmlNode::previousSiblingElement(const PkString &tagName) const
{
    if (_kind != Kind::Node) {
        return PkXmlElement();
    }
    const std::string want = tagName.PkToUtf8();
    for (pugi::xml_node c = _node.previous_sibling(); c; c = c.previous_sibling()) {
        if (c.type() == pugi::node_element && (want.empty() || want == c.name())) {
            return PkXmlElement(c, _doc);
        }
    }
    return PkXmlElement();
}

// R-25 Task 3：算法与探针实测见 PkXmlOffsetUtil.h + docs/superpowers/plans/
// R-25.md「探针实测 P15」「设计②」。属性节点、未解析/isNull() 节点恒返 -1
// ——都是探针 P15 用真实 Qt 5.15.7 实测确认过的行为，不是本任务自己的判断。

int PkXmlNode::lineNumber() const
{
    if (_kind == Kind::Attr) {
        return -1; // 探针 P15：Qt 对属性节点恒返 -1，pugixml 也没有等价的
                    // 位置概念（xml_attribute 与 xml_node 完全独立，不带
                    // offset_debug()）。
    }
    if (isNull()) {
        return -1; // 缺失/程序创建但未挂树的空句柄——Qt 恒返 -1。
    }
    if (!_doc || _doc->source.empty()) {
        return -1; // 没有原始字节可换算（比如整棵树是程序 create* 出来的，
                    // 从没经过 setContent() 解析）。
    }
    std::ptrdiff_t off = _node.offset_debug();
    if (off < 0) {
        return -1; // 未解析/程序创建的节点：pugixml 恒返 -1，与 Qt 语义在
                    // 这一点上巧合对齐（探针 P15「created (not parsed)」）。
    }
    if (_node.type() == pugi::node_pcdata || _node.type() == pugi::node_cdata) {
        off = pkXmlAdjustTextOffsetToContentEnd(_doc->source, off);
    } else {
        off = pkXmlAdjustElementOffsetToTagClose(_doc->source, off);
    }
    return pkXmlOffsetToLine(_doc->source, off);
}

int PkXmlNode::columnNumber() const
{
    if (_kind == Kind::Attr) {
        return -1;
    }
    if (isNull()) {
        return -1;
    }
    if (!_doc || _doc->source.empty()) {
        return -1;
    }
    std::ptrdiff_t off = _node.offset_debug();
    if (off < 0) {
        return -1;
    }
    if (_node.type() == pugi::node_pcdata || _node.type() == pugi::node_cdata) {
        off = pkXmlAdjustTextOffsetToContentEnd(_doc->source, off);
    } else {
        off = pkXmlAdjustElementOffsetToTagClose(_doc->source, off);
    }
    return pkXmlOffsetToColumn(_doc->source, off);
}
