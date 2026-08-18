#pragma once

#include "PkXmlNode.h"

// PkXmlAttr —— QDomAttr 的零 Qt 对应物。底层是 pugixml 的 pugi::xml_attribute
// （与 pugi::xml_node 完全独立的类型，见 PkXmlNode.h 顶部注释的 Kind::Attr
// 说明）。头文件里写全：两行取值方法，不值得单独开 .cpp。
class PkXmlAttr : public PkXmlNode
{
public:
    PkXmlAttr() = default;
    // 内部构造：供 PkXmlNode::toAttr()/PkXmlElement::attributeNode() 等跨类
    // 构造使用，不是 Qt 兼容表面的一部分。ownerNode 是属主元素——PkXmlNode
    // 的 Kind::Attr 分支把它存进基类的 `_node`，供 ownerDocument() 之类需要
    // 文档上下文的场合使用。
    PkXmlAttr(pugi::xml_node ownerNode, pugi::xml_attribute attr,
              std::shared_ptr<pugi::xml_document> doc)
        : PkXmlNode(ownerNode, attr, std::move(doc))
    {
    }

    PkString name() const { return pkXmlFromPugi(_attr.name()); }
    PkString value() const { return pkXmlFromPugi(_attr.value()); }
};
