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

// PkXmlDocRoot —— R-25 Task 3：lineNumber()/columnNumber() 的 offset→行列换算
// 需要原始输入字节，但 pugi::xml_document 内部虽然保留了一份解析用的 buffer
// 副本（`offset_debug()` 就是靠它算出来的），却没有公开 API 能把这份字节原样
// 取回。公开继承 `pugi::xml_document`（不是组合）：`PkXmlNode::_doc` 换成
// `shared_ptr<PkXmlDocRoot>` 之后，所有既有 `_doc->...`/`*_doc` 用法（本质上
// 是对 pugi::xml_document 基类子对象的调用）不用改一个字——`operator->`/
// 解引用照常工作，只是现在指向一个更派生的类型。`source` 由
// `PkXmlDocument::setContentImpl`（字节导向版本）在 `load_buffer` 之后写入，
// 不管解析成功还是失败（失败时也可能已经建出部分节点，行列换算仍然用得到）。
struct PkXmlDocRoot : public pugi::xml_document {
    std::string source;
};

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
// 创建即挂树（limbo 容器版本，R-07 全分支终审 I1 修复）：
// PkXmlDocument::createElement()/createTextNode()/createCDATASection() 把新
// 节点挂进同一棵 pugi::xml_document 树里一个隐藏的 limbo 容器子节点下（不是
// 真正悬空的，也不是文档根的直接子节点），appendChild()/insertBefore() 用
// pugixml 的 append_move()/insert_move_before() 在同一棵树内部把它从 limbo
// 搬到真正的目标位置——这是刻意选择：pugixml 的 allow_move() 硬性要求
// `parent.root() == child.root()`（src/pugixml.cpp `impl::allow_move`），跨
// xml_document 的移动一律静默失败、返回空节点；唯一跨文档转移内容的手段是
// append_copy()（深拷贝），但深拷贝会产生新的节点身份——真实调用点最常见的
// 写法 `e = doc.createElement(...); parent.appendChild(e); e.setAttribute(...)`
// 会因此静默地改在孤儿副本上而不是真正挂树的那份，不可接受。让创建出的节点
// 从一开始就活在同一棵 xml_document 里，append_move 只搬指针不拷贝，`_node`
// 句柄（哪怕是调用方手里那份自己的拷贝）因为指向同一个底层结构体，
// appendChild 之后自动"看见"新位置——不需要额外的身份同步代码。
//
// **limbo 容器为什么存在（早先版本"挂在文档根"的做法已被证伪，见 R-07 全分支
// 终审 I1）**：早先实现直接把新节点挂成文档根的子节点（与真正的
// documentElement()、doctype() 平级）。这在"调用方创建后不 appendChild 就直接
// toString()"这个真实场景下是错的——本任务自己的试接目标
// `libs/image/tests/kis_distance_information_test.cpp`
// （`testInitInfoXMLClone`）里的四个 `doc.createElement("TestN")` 全部不调用
// appendChild（只用来接住 `toXML()` 写进去的属性，之后被丢弃）。若新节点直接
// 挂在文档根，`toString()`（走 `_doc->save()`）会把这些孤儿一起序列化出来，
// 产出两个根元素的非法 XML；`documentElement()` 若先扫到孤儿也会返回错误的
// 元素。现在改为：新节点先挂进一个保留 tag 名 `#pk-xml-limbo`
// （`pkIsXmlLimboTag`/`pkIsXmlLimboNode`，XML 规范里合法元素名首字符不能是
// `#`，setContent() 解析真实 XML 不可能产生同名冲突）的隐藏容器节点下，这个
// 容器本身是文档根的子节点（保证 `allow_move` 恒成立），但
// `PkXmlNode::childNodes()`/`firstChild()`/`lastChild()`/`nextSibling()`/
// `previousSibling()`/`hasChildNodes()`/`parentNode()`（本文件 .cpp）与
// `PkXmlDocument::documentElement()`/`toString()`（`PkXmlDocument.cpp`）
// 一律跳过/无视它——孤儿节点因此不会出现在任何遍历或序列化输出里，直到真正
// 被 appendChild()/insertBefore() 搬出 limbo。
//
// 代价（比早先"挂文档根"版本更小，但仍是已知偏离）：孤儿节点的
// `parentNode()` 会规整化返回文档本身（不是 Qt 的 null，也不暴露 limbo 这个
// 内部节点——见 `PkXmlNode::parentNode()` 实现）；多个孤儿在各自被搬走之前，
// 彼此之间通过 `nextSibling()`/`previousSibling()` 是互相可见的（因为它们是
// limbo 下的亲兄弟）——真 Qt 里未挂树的孤儿节点是完全独立、互不可见的。真实
// 调用点没有发现依赖"未挂树孤儿之间互相遍历"这种写法，只在这里记录为已知
// 微小偏离，不影响"孤儿不污染最终输出"这条核心正确性要求。
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

    // R-25 Task 3：QDomNode::lineNumber()/columnNumber() 的零 Qt 对应物。
    // 算法与探针实测见 PkXmlOffsetUtil.h + docs/superpowers/plans/R-25.md
    // 「探针实测 P15」「设计②」——属性节点（Kind::Attr）与未解析/isNull()
    // 节点恒返 -1，元素节点换算到"开始标签闭合处"，文本/CDATA 节点换算到
    // "内容读完处"，与 Qt 语义对齐，不是 pugixml `offset_debug()` 的起始位置。
    int lineNumber() const;
    int columnNumber() const;

protected:
    enum class Kind { Node, Attr };

    pugi::xml_node _node;
    pugi::xml_attribute _attr;
    std::shared_ptr<PkXmlDocRoot> _doc;
    Kind _kind = Kind::Node;

    // R-25 Task 1（importNode）：C++ 的"protected 成员额外访问检查"要求非静态
    // protected 成员只能通过派生类自身（或其子类）类型的对象访问——
    // `PkXmlDocument::importNode(const PkXmlNode &importedNode, ...)` 里
    // `importedNode` 的静态类型就是 `PkXmlNode` 本身，不满足这条，直接写
    // `importedNode._node` 编不过。这里用一个 protected **静态**方法绕开
    // （静态成员调用不受该检查约束，派生类可以自由调用继承来的 protected
    // 静态方法）——只读出句柄，不放宽访问范围，PkXmlDocument 之外仍然拿不到。
    static pugi::xml_node pkRawNode(const PkXmlNode &n) { return n._node; }
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

// limbo 容器（R-07 全分支终审 I1 修复）：PkXmlDocument 用来暂存"已 createElement()
// 但还未 appendChild()"孤儿节点的隐藏容器子节点，用这个保留 tag 名标识——不是
// Qt 兼容表面的一部分，纯内部实现细节。PkXmlNode.cpp 的通用遍历方法
// （childNodes/firstChild/lastChild/nextSibling/previousSibling/hasChildNodes/
// parentNode）与 PkXmlDocument.cpp 的 documentElement()/toString() 共用这两个
// 工具，保证孤儿节点不会被任何遍历/序列化路径看见。见本文件顶部类注释。
constexpr const char *kPkXmlLimboTag = "#pk-xml-limbo";

inline bool pkIsXmlLimboNode(const pugi::xml_node &n)
{
    return n.type() == pugi::node_element && n.parent().type() == pugi::node_document
        && n.name() == std::string(kPkXmlLimboTag);
}
