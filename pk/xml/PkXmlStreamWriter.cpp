#include "PkXmlStreamWriter.h"

namespace {

// 探针 P9（Task 1 已实测确认、README §2.7）：只转义 `<`、`&`；属性值里额外
// 转义 `"`；不转义 `>` 和 `'`。writer 侧手写字符串拼接，没有 pugixml 帮忙做
// 转义，这里照 P9 的最小转义集自己实现一遍——与 Task 1 DOM 侧（走 pugixml
// 内建的 save()）语义一致，只是实现手段不同。
std::string pkEscapeText(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '<':
            out += "&lt;";
            break;
        case '&':
            out += "&amp;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

std::string pkEscapeAttribute(const std::string &s)
{
    std::string out;
    out.reserve(s.size());
    for (char c : s) {
        switch (c) {
        case '<':
            out += "&lt;";
            break;
        case '&':
            out += "&amp;";
            break;
        case '"':
            out += "&quot;";
            break;
        default:
            out += c;
        }
    }
    return out;
}

} // namespace

PkXmlStreamWriter::PkXmlStreamWriter(PkString *output)
    : _output(output)
{
}

void PkXmlStreamWriter::_closeStartTagIfOpen()
{
    if (_startTagOpen) {
        _buf += '>';
        _startTagOpen = false;
    }
}

void PkXmlStreamWriter::_sync()
{
    if (_output) {
        *_output = PkString::PkFromUtf8(_buf.c_str(), static_cast<int>(_buf.size()));
    }
}

void PkXmlStreamWriter::writeStartDocument()
{
    _buf += "<?xml version=\"1.0\" encoding=\"UTF-8\"?>";
    _sync();
}

void PkXmlStreamWriter::writeEndDocument()
{
    // 探针 P10：两种模式的完整输出结尾都恰好一个换行——紧凑模式本身不含任何
    // 换行，这里补一个；autoFormatting 模式最后一个 EndElement 已经换过行，
    // 不能重复补，所以只在缓冲区还没以 '\n' 收尾时才补。
    if (_buf.empty() || _buf.back() != '\n') {
        _buf += '\n';
    }
    _sync();
}

void PkXmlStreamWriter::writeStartElement(const PkString &name)
{
    if (!_openElements.empty()) {
        _openElements.back().hasChildElement = true;
    }
    _closeStartTagIfOpen();

    if (_autoFormatting) {
        _buf += '\n';
        _buf += std::string(_openElements.size() * 4, ' ');
    }

    _buf += '<';
    _buf += name.PkToUtf8();

    OpenElement e;
    e.name = name;
    _openElements.push_back(e);
    _startTagOpen = true;
    _sync();
}

void PkXmlStreamWriter::writeEndElement()
{
    if (_openElements.empty()) {
        _sync();
        return;
    }

    const OpenElement top = _openElements.back();
    _openElements.pop_back();

    if (_startTagOpen) {
        // 起始标签还没关闭 = 内部什么都没写（既没有子元素也没有文本）：
        // 直接补 `>` 紧跟 `</tag>`，与只含文本的情形一样不换行——Qt 默认不会
        // 把空元素自折叠成 `<tag/>`（那是 writeEmptyElement() 的行为，用量表
        // 实测调用点为 0，不实现）。
        _buf += '>';
        _startTagOpen = false;
    } else if (top.hasChildElement && _autoFormatting) {
        _buf += '\n';
        _buf += std::string(_openElements.size() * 4, ' ');
    }

    _buf += "</";
    _buf += top.name.PkToUtf8();
    _buf += '>';
    _sync();
}

void PkXmlStreamWriter::writeAttribute(const PkString &name, const PkString &value)
{
    // 只在起始标签还开着时有意义，见头文件类注释——本类不做误用防御。
    _buf += ' ';
    _buf += name.PkToUtf8();
    _buf += "=\"";
    _buf += pkEscapeAttribute(value.PkToUtf8());
    _buf += '"';
    _sync();
}

void PkXmlStreamWriter::writeCharacters(const PkString &text)
{
    _closeStartTagIfOpen();
    _buf += pkEscapeText(text.PkToUtf8());
    _sync();
}
