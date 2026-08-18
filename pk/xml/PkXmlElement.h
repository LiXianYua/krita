#pragma once

#include "PkXmlAttr.h"
#include "PkXmlNode.h"
#include "PkXmlNodeList.h"

// PkXmlElement —— QDomElement 的零 Qt 对应物。属性/子元素查询都直通 pugixml，
// 只有两类需要本任务自己写逻辑（pugixml 不免费提供）：
//   - `elementsByTagName`（探针 P7）：递归全子树 DFS，不是仅直接子元素；
//   - `text()`（探针 P8）：递归拼接全部后代文本，不是仅直接子文本；
//   - `localName()`/`namespaceURI()`/`attributeNS()`：pugixml 不做命名空间解析
//     （`xmlns:foo="uri"` 被当成普通属性、`foo:tag` 被当成字面 tag name），
//     这里手动拆 `prefix:localname` 再沿祖先链查最近的 `xmlns[:prefix]` 属性
//     解析 URI——只覆盖真实调用点用到的固定前缀场景（见 README），不是完整
//     XML Namespaces 规范实现。
class PkXmlElement : public PkXmlNode
{
public:
    PkXmlElement() = default;
    // 内部构造：供 PkXmlNode::toElement()/PkXmlDocument::createElement() 等
    // 跨类构造使用，不是 Qt 兼容表面的一部分。
    PkXmlElement(pugi::xml_node node, std::shared_ptr<pugi::xml_document> doc)
        : PkXmlNode(node, std::move(doc))
    {
    }

    PkString tagName() const;

    PkString attribute(const PkString &name, const PkString &defaultValue = PkString()) const;
    void setAttribute(const PkString &name, const PkString &value);
    bool hasAttribute(const PkString &name) const;
    void removeAttribute(const PkString &name);
    PkXmlAttr attributeNode(const PkString &name) const;
    PkString attributeNS(const PkString &nsUri, const PkString &name,
                          const PkString &defaultValue = PkString()) const;

    PkXmlElement firstChildElement(const PkString &tagName = PkString()) const;
    PkXmlElement lastChildElement(const PkString &tagName = PkString()) const;
    PkXmlElement nextSiblingElement(const PkString &tagName = PkString()) const;
    PkXmlElement previousSiblingElement(const PkString &tagName = PkString()) const;

    PkString text() const;
    PkXmlNodeList elementsByTagName(const PkString &tagName) const;

    PkString localName() const;
    PkString namespaceURI() const;
};
