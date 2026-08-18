#pragma once

#include <memory>
#include <string>

#include <pugixml.hpp>

#include "../string/PkString.h"

class PkXmlElement;
class PkXmlText;
class PkXmlCDATASection;
class PkXmlAttr;
class PkXmlNodeList;
class PkXmlDocument;

// PkXmlNode —— QDomNode 的零 Qt 对应物，底层用 pugixml 的 pugi::xml_node 句柄。
//
// 内部表示（决定了每个方法怎么实现）：
//   - `_node` 持有一个 pugi::xml_node 值句柄——pugixml 的句柄本身很轻（一个指向
//     内部节点结构体的指针），不持有内存；
//   - `_doc` 持有一个指向 pugi::xml_document 的 shared_ptr——不是为了 COW，是因为
//     pugi::xml_document 拥有整棵树的内存池，`_node` 句柄失效的前提是它的
//     xml_document 还活着；`_doc` 用 shared_ptr 还有第二个理由：pugixml 把首页
//     内存结构体嵌在 xml_document 对象自身里，xml_document 一旦被拷贝/移动（比如
//     按值存进 std::vector）内部指针就会失效——shared_ptr 保证这个堆上对象的
//     地址终生不变。
//   - `_attr` + `_kind==Kind::Attr`：pugixml 把属性（pugi::xml_attribute）设计成
//     与 pugi::xml_node 完全独立的类型，不像 Qt DOM 把 QDomAttr 也纳入 QDomNode
//     继承体系。为了不改真实调用点一个字（它们把 QDomAttr 当 QDomNode 用），
//     这里用一个判别位混合两种底层句柄：Kind::Node 时 `_node` 是真正的节点；
//     Kind::Attr 时 `_node` 存**属主元素**（给 ownerDocument() 之类需要文档
//     上下文的场合用），`_attr` 才是真正的属性句柄。
//
// 创建即挂树：PkXmlDocument::createElement()/createTextNode()/
// createCDATASection() 把新节点作为文档根的子节点立即挂进同一棵 pugi::xml_document
// 树（不是真正悬空的），appendChild()/insertBefore() 用 pugixml 的
// append_move()/insert_move_before() 在同一棵树内部搬动它——这是刻意选择：
// pugixml 的 allow_move() 硬性要求 `parent.root() == child.root()`
// （src/pugixml.cpp `impl::allow_move`），跨 xml_document 的移动一律静默失败、
// 返回空节点；唯一跨文档转移内容的手段是 append_copy()（深拷贝），但深拷贝会
// 产生新的节点身份——真实调用点最常见的写法
// `e = doc.createElement(...); parent.appendChild(e); e.setAttribute(...)`
// 会因此静默地改在孤儿副本上而不是真正挂树的那份，不可接受。让创建出的节点
// 从一开始就活在同一棵 xml_document 里，append_move 只搬指针不拷贝，`_node`
// 句柄（哪怕是调用方手里那份自己的拷贝）因为指向同一个底层结构体，
// appendChild 之后自动"看见"新位置——不需要额外的身份同步代码。
//
// 已知偏离（README 已记录）：createElement() 返回的节点在被真正
// appendChild()/insertBefore() 之前，parentNode() 会返回文档本身而不是 Qt 那样
// 的 null——因为它已经是文档根的子节点了，只是还没搬到目标位置。真实调用点
// 一律是"创建后立刻 appendChild"，不存在中间态被查询的场景。
class PkXmlNode
{
public:
    PkXmlNode();
    PkXmlNode(const PkXmlNode &) = default;
    PkXmlNode(PkXmlNode &&) noexcept = default;
    PkXmlNode &operator=(const PkXmlNode &) = default;
    PkXmlNode &operator=(PkXmlNode &&) noexcept = default;
    virtual ~PkXmlNode() = default;

    // 内部构造：供 PkXmlDocument/PkXmlElement 等工厂方法与 toElement() 系列
    // 转型方法跨类构造彼此使用，不是 Qt 兼容表面的一部分——真实调用点只走
    // 下面的公开工厂方法/转型方法，从不直接传 pugi::xml_node。
    PkXmlNode(pugi::xml_node node, std::shared_ptr<pugi::xml_document> doc);
    PkXmlNode(pugi::xml_node ownerNode, pugi::xml_attribute attr,
              std::shared_ptr<pugi::xml_document> doc);

    bool isNull() const;
    bool isElement() const;
    bool isText() const;
    bool isCDATASection() const;
    bool isAttr() const;

    PkXmlElement toElement() const;
    PkXmlText toText() const;
    PkXmlCDATASection toCDATASection() const;
    PkXmlAttr toAttr() const;

    PkString nodeName() const;

    PkXmlNodeList childNodes() const;
    PkXmlNode firstChild() const;
    PkXmlNode lastChild() const;
    PkXmlNode nextSibling() const;
    PkXmlNode previousSibling() const;
    PkXmlNode parentNode() const;
    bool hasChildNodes() const;

    PkXmlNode appendChild(const PkXmlNode &newChild);
    PkXmlNode insertBefore(const PkXmlNode &newChild, const PkXmlNode &refChild);
    bool removeChild(const PkXmlNode &oldChild);

    PkXmlDocument ownerDocument() const;

    // QDomNode 自 Qt 4.1 起自带的四个便捷方法（不是 QDomElement 独有）——
    // 跳过非元素兄弟/子节点直接找元素，效果等价于先 toElement() 再调用
    // PkXmlElement 上同名方法，但不需要调用方自己转型。
    //
    // R-07 Task 3 试接 `libs/global/kis_dom_utils.cpp` 时实测压出来的缺口：
    // `findElementByAttribute(QDomNode parent, ...)` 直接对形参类型是
    // QDomNode（未转型）的 `parent` 调 `parent.firstChildElement(tag)` /
    // `e.nextSiblingElement(tag)`——这是真实、常见的 Qt 用法，Task 1 交付时
    // 只在 PkXmlElement 上给了这四个方法，PkXmlNode 上没有对应物。
    PkXmlElement firstChildElement(const PkString &tagName = PkString()) const;
    PkXmlElement lastChildElement(const PkString &tagName = PkString()) const;
    PkXmlElement nextSiblingElement(const PkString &tagName = PkString()) const;
    PkXmlElement previousSiblingElement(const PkString &tagName = PkString()) const;

protected:
    enum class Kind { Node, Attr };

    pugi::xml_node _node;
    pugi::xml_attribute _attr;
    std::shared_ptr<pugi::xml_document> _doc;
    Kind _kind = Kind::Node;
};

// utf8 <-> PkString 互转——pugixml 默认（非 PUGIXML_WCHAR_MODE）以 UTF-8 存取
// name()/value()，PkString 的对应互操作是 PkFromUtf8()/PkToUtf8()。放在头文件里
// 内联，是本目录内多个 .cpp 共用的小工具，不属于任何一个类的公开 API。
inline PkString pkXmlFromPugi(const char *utf8)
{
    if (!utf8) {
        return PkString();
    }
    return PkString::PkFromUtf8(utf8, static_cast<int>(std::char_traits<char>::length(utf8)));
}
