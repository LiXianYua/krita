#pragma once

#include "PkXmlNode.h"

// PkXmlDocumentType —— QDomDocumentType 的零 Qt 对应物，"最小实现"：只覆盖
// 真实调用点用到的 `name()`（见 pk/xml/README.md 的用量表）。pugixml 的
// node_doctype 节点只把 DOCTYPE 声明的原始文本整段存进 value()（例如
// `note SYSTEM "Note.dtd"`），不像 Qt 那样拆好 name/publicId/systemId 三个
// 字段——`name()` 由 PkXmlDocument::doctype() 解析 value() 的第一个空白分隔
// token 得到；PkXmlImplementation::createDocumentType() 构造的实例则直接
// 记住调用方传入的 qName，两条路径都汇聚到同一个 `_name` 字段。
class PkXmlDocumentType : public PkXmlNode
{
public:
    PkXmlDocumentType() = default;
    PkXmlDocumentType(pugi::xml_node node, std::shared_ptr<pugi::xml_document> doc, PkString name)
        : PkXmlNode(node, std::move(doc)), _name(std::move(name))
    {
    }

    PkString name() const { return _name; }

    // 隐藏（非虚，不是覆盖）基类的 isNull()：PkXmlImplementation::createDocumentType()
    // 产出的实例常常不背靠真实 pugi::xml_node（纯值持有），有没有 doctype 用
    // `_name` 是否为空判定，不是看 `_node` 是否为空节点。
    bool isNull() const { return _name.isEmpty(); }

private:
    PkString _name;
};

// PkXmlImplementation —— QDomImplementation 的零 Qt 对应物，"最小实现"：只覆盖
// 真实调用点用到的 `createDocumentType()`。
class PkXmlImplementation
{
public:
    PkXmlImplementation() = default;

    PkXmlDocumentType createDocumentType(const PkString &qName, const PkString &publicId,
                                          const PkString &systemId) const
    {
        (void)publicId; // 未被任何真实调用点用到（用量表未出现 publicId/systemId 取值），
        (void)systemId; // 最小实现按 brief 只存 name，这两个参数保留仅为签名对齐 Qt。
        return PkXmlDocumentType(pugi::xml_node(), nullptr, qName);
    }
};
