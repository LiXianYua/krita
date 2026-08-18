#include "PkXmlNode.h"

#include "PkXmlAttr.h"
#include "PkXmlCDATASection.h"
#include "PkXmlDocument.h"
#include "PkXmlElement.h"
#include "PkXmlNodeList.h"
#include "PkXmlText.h"

PkXmlNode::PkXmlNode() = default;

PkXmlNode::PkXmlNode(pugi::xml_node node, std::shared_ptr<pugi::xml_document> doc)
    : _node(node), _doc(std::move(doc)), _kind(Kind::Node)
{
}

PkXmlNode::PkXmlNode(pugi::xml_node ownerNode, pugi::xml_attribute attr,
                      std::shared_ptr<pugi::xml_document> doc)
    : _node(ownerNode), _attr(attr), _doc(std::move(doc)), _kind(Kind::Attr)
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
        list.PkAppend(PkXmlNode(c, _doc));
    }
    return list;
}

PkXmlNode PkXmlNode::firstChild() const
{
    if (_kind != Kind::Node) {
        return PkXmlNode();
    }
    return PkXmlNode(_node.first_child(), _doc);
}

PkXmlNode PkXmlNode::lastChild() const
{
    if (_kind != Kind::Node) {
        return PkXmlNode();
    }
    return PkXmlNode(_node.last_child(), _doc);
}

PkXmlNode PkXmlNode::nextSibling() const
{
    if (_kind != Kind::Node) {
        return PkXmlNode();
    }
    return PkXmlNode(_node.next_sibling(), _doc);
}

PkXmlNode PkXmlNode::previousSibling() const
{
    if (_kind != Kind::Node) {
        return PkXmlNode();
    }
    return PkXmlNode(_node.previous_sibling(), _doc);
}

PkXmlNode PkXmlNode::parentNode() const
{
    if (_kind != Kind::Node) {
        return PkXmlNode();
    }
    return PkXmlNode(_node.parent(), _doc);
}

bool PkXmlNode::hasChildNodes() const
{
    return _kind == Kind::Node && static_cast<bool>(_node.first_child());
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
