#pragma once

#include "PkXmlText.h"

// PkXmlCDATASection —— QDomCDATASection 的零 Qt 对应物。Qt 里 QDomCDATASection
// 是 QDomText 的子类且不新增任何成员（只是 nodeType() 不同）；底层 pugixml 的
// node_cdata 节点同样用 value() 存内容，`data()` 直接继承 PkXmlText 的实现，
// 不需要重写。头文件里写全：没有任何新增逻辑，不值得单独开 .cpp。
class PkXmlCDATASection : public PkXmlText
{
public:
    PkXmlCDATASection() = default;
    // 内部构造：供 PkXmlNode::toCDATASection()/PkXmlDocument::createCDATASection()
    // 等跨类构造使用，不是 Qt 兼容表面的一部分。
    PkXmlCDATASection(pugi::xml_node node, std::shared_ptr<pugi::xml_document> doc)
        : PkXmlText(node, std::move(doc))
    {
    }
};
