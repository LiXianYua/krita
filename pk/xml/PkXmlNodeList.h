#pragma once

#include <vector>

#include "PkXmlNode.h"

// PkXmlNodeList —— QDomNodeList 的零 Qt 对应物：一份不可变的 PkXmlNode 快照
// （与 Qt 的"实时视图"语义不同，但真实调用点全部是"取一次、遍历一次"，见
// pk/xml/README.md 已知偏离清单）。头文件里写全，不单独开 .cpp —— 全部方法
// 只是对内部 std::vector<PkXmlNode> 的直通存取，没有需要摊到实现文件里的逻辑。
class PkXmlNodeList
{
public:
    PkXmlNodeList() = default;
    explicit PkXmlNodeList(std::vector<PkXmlNode> nodes) : _nodes(std::move(nodes)) {}

    int count() const { return static_cast<int>(_nodes.size()); }
    int size() const { return count(); }    // QDomNodeList::size() 起别名转发
    int length() const { return count(); }  // QDomNodeList::length() 起别名转发

    PkXmlNode at(int index) const
    {
        if (index < 0 || index >= count()) {
            return PkXmlNode();
        }
        return _nodes[static_cast<std::size_t>(index)];
    }
    PkXmlNode item(int index) const { return at(index); }  // QDomNodeList::item() 起别名转发

    // 内部工具：由 PkXmlNode::childNodes()/PkXmlElement::elementsByTagName() 等
    // 遍历实现用来边收集边追加，不是 Qt 兼容表面的一部分。
    void PkAppend(const PkXmlNode &node) { _nodes.push_back(node); }

private:
    std::vector<PkXmlNode> _nodes;
};
