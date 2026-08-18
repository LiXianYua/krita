#include "PkXmlStreamReader.h"

#include <cstring>

namespace {

// utf8 -> PkString。刻意不 #include PkXmlNode.h 复用它现成的
// `pkXmlFromPugi()`：README 已声明 Task 2 只复用 pugixml 这个库本身，不消费
// Task 1 的 C++ 类型，拉 PkXmlNode.h 进来会把整个 DOM 类族的编译依赖也带上。
PkString pkStreamFromUtf8(const char *utf8)
{
    if (!utf8) {
        return PkString();
    }
    return PkString::PkFromUtf8(utf8, static_cast<int>(std::strlen(utf8)));
}

} // namespace

PkXmlStreamReader::PkXmlStreamReader(const PkString &data)
{
    const std::string utf8 = data.PkToUtf8();
    // parse_default | parse_doctype：与 Task 1 PkXmlDocument::setContent 同一套
    // 解析标志（README §2.5），保持两边对同一份输入的解析行为一致。
    const pugi::xml_parse_result result =
        _doc.load_buffer(utf8.data(), utf8.size(), pugi::parse_default | pugi::parse_doctype);

    _parsedOk = static_cast<bool>(result);
    if (!_parsedOk) {
        _hasError = true;
        _errorString = pkStreamFromUtf8(result.description());
        return;
    }

    for (pugi::xml_node c = _doc.first_child(); c; c = c.next_sibling()) {
        if (c.type() == pugi::node_element) {
            _root = c;
            break;
        }
    }
}

void PkXmlStreamReader::raiseError(const PkString &message)
{
    // Qt 语义：raiseError() 之后 hasError()==true、errorString() 是调用点给的
    // message、且 reader 进入终止状态（atEnd()==true，之后的 readNext() 不再
    // 继续遍历树）——真实调用点（KoColorSet.cpp）的写法都是
    // `xml->raiseError(...); return;`，紧跟着从当前解析函数返回，外层
    // `while (!xml.atEnd())` 循环据此在下一次检查时退出，不会继续用一棵
    // 已经被判定为非法的数据往下读。
    _hasError = true;
    _errorString = message;
    _atEnd = true;
}

PkXmlStreamAttributes PkXmlStreamReader::_collectAttributes(const pugi::xml_node &element)
{
    PkXmlStreamAttributes attrs;
    for (pugi::xml_attribute a = element.first_attribute(); a; a = a.next_attribute()) {
        attrs.PkAppend(pkStreamFromUtf8(a.name()), pkStreamFromUtf8(a.value()));
    }
    return attrs;
}

PkXmlStreamReader::TokenType PkXmlStreamReader::readNext()
{
    if (!_parsedOk) {
        // 探针 P11 之外的场景：构造时解析就失败了。Qt 在这种输入下最终会
        // 停在 hasError()==true 且 atEnd()==true——真实调用点的
        // `while (!xml.atEnd())` 惯用法据此立刻退出循环，不会死循环。
        _tokenType = Invalid;
        _name = PkString();
        _text = PkString();
        _attributes = PkXmlStreamAttributes();
        _atEnd = true;
        return _tokenType;
    }

    if (!_started) {
        _started = true;
        if (_root) {
            Frame f;
            f.element = _root;
            _stack.push_back(f);
        }
        _tokenType = StartDocument;
        _name = PkString();
        _text = PkString();
        _attributes = PkXmlStreamAttributes();
        return _tokenType;
    }

    if (_stack.empty()) {
        // 全部元素都已经关闭——发出终止哨兵，之后 atEnd() 为真。
        _tokenType = EndDocument;
        _name = PkString();
        _text = PkString();
        _attributes = PkXmlStreamAttributes();
        _atEnd = true;
        return _tokenType;
    }

    Frame &top = _stack.back();

    if (!top.entered) {
        // 刚进栈、还没发出过它自己的 StartElement。
        top.entered = true;
        top.cursor = top.element.first_child();
        _tokenType = StartElement;
        _name = pkStreamFromUtf8(top.element.name());
        _text = PkString();
        _attributes = _collectAttributes(top.element);
        return _tokenType;
    }

    // 跳过 parse_default|parse_doctype 产出范围之外、遍历用不到的节点类型
    // （理论上不会出现在元素子树里，防御性跳过而不是断言——与 Task 1
    // PkXmlElement::text() 的收窄口径一致，只认 node_element/node_pcdata/
    // node_cdata）。
    while (top.cursor
           && top.cursor.type() != pugi::node_element
           && top.cursor.type() != pugi::node_pcdata
           && top.cursor.type() != pugi::node_cdata) {
        top.cursor = top.cursor.next_sibling();
    }

    if (top.cursor) {
        const pugi::xml_node child = top.cursor;
        top.cursor = top.cursor.next_sibling();

        if (child.type() == pugi::node_element) {
            // 这一次调用已经把 child 的 StartElement 发出去了（下面直接赋值
            // _tokenType）——新 Frame 必须以 entered=true、cursor 指向它自己
            // 的第一个子节点入栈，否则下一次 readNext() 会在 `!top.entered`
            // 分支里把同一个 StartElement 再发一遍。
            Frame f;
            f.element = child;
            f.entered = true;
            f.cursor = child.first_child();
            _stack.push_back(f);
            _tokenType = StartElement;
            _name = pkStreamFromUtf8(child.name());
            _text = PkString();
            _attributes = _collectAttributes(child);
        } else {
            _tokenType = Characters;
            _name = PkString();
            _text = pkStreamFromUtf8(child.value());
            _attributes = PkXmlStreamAttributes();
        }
        return _tokenType;
    }

    // top 的子节点已经遍历完——发出它的 EndElement 并出栈。
    _tokenType = EndElement;
    _name = pkStreamFromUtf8(top.element.name());
    _text = PkString();
    _attributes = PkXmlStreamAttributes();
    _stack.pop_back();
    return _tokenType;
}
