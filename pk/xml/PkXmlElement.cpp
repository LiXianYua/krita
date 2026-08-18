#include "PkXmlElement.h"

#include <string>

namespace {

// 沿祖先链（含自身）查最近的 `xmlns[:prefix]` 声明——prefix 为空时查默认
// 命名空间 `xmlns`。真实调用点只用到固定前缀（KoXmlNS::draw/svg/manifest 等），
// 不需要完整 XML Namespaces 规范覆盖率。
std::string pkResolveNamespaceUri(pugi::xml_node node, const std::string &prefix)
{
    const std::string attrName = prefix.empty() ? std::string("xmlns") : ("xmlns:" + prefix);
    for (pugi::xml_node n = node; n; n = n.parent()) {
        pugi::xml_attribute a = n.attribute(attrName.c_str());
        if (a) {
            return a.value();
        }
    }
    return std::string();
}

void pkSplitQName(const std::string &qName, std::string &prefix, std::string &localName)
{
    const auto pos = qName.find(':');
    if (pos == std::string::npos) {
        prefix.clear();
        localName = qName;
    } else {
        prefix = qName.substr(0, pos);
        localName = qName.substr(pos + 1);
    }
}

// 探针 P8：QDomElement::text() 是递归拼接全部后代文本，不只是直接子文本。
// 只下钻 node_element 子节点——comment/pi 等类型不在默认解析标志（parse_default）
// 的产出范围内，真实调用点也用不到。
void pkAppendText(const pugi::xml_node &n, std::string &out)
{
    for (pugi::xml_node c = n.first_child(); c; c = c.next_sibling()) {
        if (c.type() == pugi::node_pcdata || c.type() == pugi::node_cdata) {
            out += c.value();
        } else if (c.type() == pugi::node_element) {
            pkAppendText(c, out);
        }
    }
}

// 探针 P7：elementsByTagName 是递归全子树查找，不是仅直接子元素——手写前序
// DFS，命中元素自己的子树里可能还有同名元素，不能提前剪枝。
void pkCollectByTagName(const pugi::xml_node &n, const std::string &tag,
                         const std::shared_ptr<pugi::xml_document> &doc,
                         std::vector<PkXmlNode> &out)
{
    for (pugi::xml_node c = n.first_child(); c; c = c.next_sibling()) {
        if (c.type() != pugi::node_element) {
            continue;
        }
        if (tag == c.name()) {
            out.push_back(PkXmlNode(c, doc));
        }
        pkCollectByTagName(c, tag, doc, out);
    }
}

} // namespace

PkString PkXmlElement::tagName() const
{
    return pkXmlFromPugi(_node.name());
}

PkString PkXmlElement::attribute(const PkString &name, const PkString &defaultValue) const
{
    pugi::xml_attribute a = _node.attribute(name.PkToUtf8().c_str());
    if (!a) {
        return defaultValue;
    }
    // P5：key 存在但值为空串时返回空串，不走 defaultValue 分支——
    // attribute() 与 hasAttribute() 语义独立，不能用"返回值是否等于默认值"
    // 判断 key 存在与否。
    return pkXmlFromPugi(a.value());
}

void PkXmlElement::setAttribute(const PkString &name, const PkString &value)
{
    const std::string n = name.PkToUtf8();
    pugi::xml_attribute a = _node.attribute(n.c_str());
    if (!a) {
        a = _node.append_attribute(n.c_str());
    }
    a.set_value(value.PkToUtf8().c_str());
}

bool PkXmlElement::hasAttribute(const PkString &name) const
{
    return static_cast<bool>(_node.attribute(name.PkToUtf8().c_str()));
}

void PkXmlElement::removeAttribute(const PkString &name)
{
    _node.remove_attribute(name.PkToUtf8().c_str());
}

PkXmlAttr PkXmlElement::attributeNode(const PkString &name) const
{
    pugi::xml_attribute a = _node.attribute(name.PkToUtf8().c_str());
    if (!a) {
        return PkXmlAttr();
    }
    return PkXmlAttr(_node, a, _doc);
}

PkString PkXmlElement::attributeNS(const PkString &nsUri, const PkString &name,
                                    const PkString &defaultValue) const
{
    const std::string wantLocal = name.PkToUtf8();
    const std::string wantNs = nsUri.PkToUtf8();
    for (pugi::xml_attribute a = _node.first_attribute(); a; a = a.next_attribute()) {
        const std::string an = a.name();
        if (an == "xmlns" || an.rfind("xmlns:", 0) == 0) {
            continue; // 命名空间声明本身不算普通属性
        }
        std::string prefix, local;
        pkSplitQName(an, prefix, local);
        if (local != wantLocal) {
            continue;
        }
        if (pkResolveNamespaceUri(_node, prefix) == wantNs) {
            return pkXmlFromPugi(a.value());
        }
    }
    return defaultValue;
}

PkXmlElement PkXmlElement::firstChildElement(const PkString &tagName) const
{
    const std::string want = tagName.PkToUtf8();
    for (pugi::xml_node c = _node.first_child(); c; c = c.next_sibling()) {
        if (c.type() == pugi::node_element && (want.empty() || want == c.name())) {
            return PkXmlElement(c, _doc);
        }
    }
    return PkXmlElement();
}

PkXmlElement PkXmlElement::lastChildElement(const PkString &tagName) const
{
    const std::string want = tagName.PkToUtf8();
    for (pugi::xml_node c = _node.last_child(); c; c = c.previous_sibling()) {
        if (c.type() == pugi::node_element && (want.empty() || want == c.name())) {
            return PkXmlElement(c, _doc);
        }
    }
    return PkXmlElement();
}

PkXmlElement PkXmlElement::nextSiblingElement(const PkString &tagName) const
{
    const std::string want = tagName.PkToUtf8();
    for (pugi::xml_node c = _node.next_sibling(); c; c = c.next_sibling()) {
        if (c.type() == pugi::node_element && (want.empty() || want == c.name())) {
            return PkXmlElement(c, _doc);
        }
    }
    return PkXmlElement();
}

PkXmlElement PkXmlElement::previousSiblingElement(const PkString &tagName) const
{
    const std::string want = tagName.PkToUtf8();
    for (pugi::xml_node c = _node.previous_sibling(); c; c = c.previous_sibling()) {
        if (c.type() == pugi::node_element && (want.empty() || want == c.name())) {
            return PkXmlElement(c, _doc);
        }
    }
    return PkXmlElement();
}

PkString PkXmlElement::text() const
{
    std::string out;
    pkAppendText(_node, out);
    return PkString::PkFromUtf8(out.c_str(), static_cast<int>(out.size()));
}

PkXmlNodeList PkXmlElement::elementsByTagName(const PkString &tagName) const
{
    std::vector<PkXmlNode> out;
    pkCollectByTagName(_node, tagName.PkToUtf8(), _doc, out);
    return PkXmlNodeList(std::move(out));
}

PkString PkXmlElement::localName() const
{
    std::string prefix, local;
    pkSplitQName(_node.name(), prefix, local);
    return pkXmlFromPugi(local.c_str());
}

PkString PkXmlElement::namespaceURI() const
{
    std::string prefix, local;
    pkSplitQName(_node.name(), prefix, local);
    return pkXmlFromPugi(pkResolveNamespaceUri(_node, prefix).c_str());
}
