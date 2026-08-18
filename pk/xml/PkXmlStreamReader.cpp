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
        // 探针实测（见 R-07 Task 2 报告"raiseError 语义核实"一节，同一套
        // 冻结状态适用于构造期解析失败）：tokenType() 恒为 Invalid，
        // atEnd() 恒为 true，readNext() 之后不再变化。
        _tokenType = Invalid;
        _atEnd = true;
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
    // 探针实测（Qt 5.15.7，`libQt5Core.so.5.15.7`，见 R-07 Task 2 报告
    // "raiseError 语义核实"一节）：raiseError() 之后不需要再调用 readNext()，
    // tokenType() 立刻变成 Invalid、atEnd()/hasError() 立刻变成
    // true、errorString() 立刻是传入的 message——**但 name()/text()/
    // attributes() 不变**，仍然是报错前最后一次成功 readNext() 留下的值
    // （探针原始输出：`immediately after raiseError (no readNext yet):
    // tokenType=Invalid name=[root] text=[] attrs.count()=1 atEnd=1`）。
    // 之后任何一次 readNext() 都不再继续遍历、不再改变这些值——见
    // readNext() 顶部的 `if (_hasError)` 短路分支。
    _hasError = true;
    _errorString = message;
    _atEnd = true;
    _tokenType = Invalid;
}

bool PkXmlStreamReader::readNextStartElement()
{
    // Qt 语义：一路 readNext()，跳过 Characters（本类的 token 集合里没有
    // Comment/PI/EntityReference，见 README §7「明确排除」），遇到
    // StartElement 返回 true；遇到 EndElement（当前元素自己的收尾）/
    // EndDocument/Invalid（出错或提前结束，防止死循环）返回 false。
    while (true) {
        const TokenType t = readNext();
        if (t == StartElement) {
            return true;
        }
        if (t == EndElement || t == EndDocument || t == Invalid) {
            return false;
        }
        // 其余（Characters/StartDocument/NoToken）继续跳过。
    }
}

void PkXmlStreamReader::skipCurrentElement()
{
    // 前提（与真实调用点一致）：当前 tokenType() 是 StartElement——把它自己
    // 计成深度 1，readNext() 到深度归零那一次 EndElement（即它自己配对的
    // 收尾标签）为止，中途遇到的子 StartElement/EndElement 只增减深度，不
    // 单独处理。EndDocument/Invalid 兜底跳出，防止输入提前结束时死循环。
    int depth = 1;
    while (depth > 0) {
        const TokenType t = readNext();
        if (t == StartElement) {
            ++depth;
        } else if (t == EndElement) {
            --depth;
        } else if (t == EndDocument || t == Invalid) {
            break;
        }
    }
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
    if (_hasError) {
        // 状态已经在构造失败分支或 raiseError() 里冻结在报错前一刻——探针
        // 实测：之后任意多次 readNext() 都恒返回 Invalid，不再继续遍历、
        // 不再改变 name()/text()/attributes() 的值（不能在这里清空它们）。
        // 真实调用点的 `while (!xml.atEnd())` 惯用法据此在下一次检查时
        // 退出循环，不会死循环。
        return Invalid;
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
