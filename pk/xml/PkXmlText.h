#pragma once

#include "PkXmlNode.h"

// PkXmlText —— QDomText 的零 Qt 对应物。底层是 pugixml 的 node_pcdata 节点，
// `data()` 直接转发 pugi::xml_node::value()（PCDATA 节点的 value() 就是文本
// 内容本身）。头文件里写全：一行方法体，不值得单独开 .cpp。
class PkXmlText : public PkXmlNode
{
public:
    PkXmlText() = default;
    // 内部构造：供 PkXmlNode::toText()/PkXmlDocument::createTextNode() 等跨类
    // 构造使用，不是 Qt 兼容表面的一部分。
    PkXmlText(pugi::xml_node node, std::shared_ptr<pugi::xml_document> doc)
        : PkXmlNode(node, std::move(doc))
    {
    }

    PkString data() const { return pkXmlFromPugi(_node.value()); }
};
