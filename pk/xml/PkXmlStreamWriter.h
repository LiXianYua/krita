#pragma once

#include <string>
#include <vector>

#include "../string/PkString.h"

// PkXmlStreamWriter —— QXmlStreamWriter 的零 Qt 对应物。
//
// 不建树：pugixml 没有"边写边序列化"的流式 writer API（它只会先建一棵
// pugi::xml_document 再整体 save）。真实调用点的写入顺序
// （writeStartDocument→writeStartElement→writeAttribute→writeStartElement→
// …→writeEndElement→writeEndDocument）本身就是一个可以直接手写字符串拼接
// 状态机模拟的顺序流，不需要真的建一棵 DOM 树再序列化——这是刻意的范围决策
// （见线级 plan Task2「关键实现说明 1」），不是偷懒。
//
// 内部只维护一个"当前打开的元素名"栈（`_openElements`）：`writeStartElement`
// push、`writeEndElement` pop 并写 `</tag>`。
//
// `writeAttribute` 只能在"刚 `writeStartElement` 之后、还没写任何子内容之前"
// 调用——这是 Qt 的真实约束（pugixml 完全不涉及这层，因为它是本类自己手写的
// 状态机，不是 pugixml 帮忙管的）：一旦某次 `writeStartElement`/
// `writeCharacters`/`writeEndElement` 关闭了当前起始标签（写出了那个 `>`），
// 再调用 `writeAttribute` 只会把属性文本拼接到错误的位置、产生不合法
// XML——与 Qt 的行为一致（Qt 同样不保证误用顺序时输出合法），本类不做额外的
// 顺序防御。
//
// 探针 P10 的两组实测输出（字节级对齐，`test_stream_writer.cpp` 逐字节断言）：
//   无 autoFormatting：紧凑单行，结尾恰好一个换行；
//   autoFormatting(true)：4 空格缩进，含子元素的元素起止标签各占一行、
//   只含文本的元素起止标签仍在同一行（不强制拆行）。
class PkXmlStreamWriter
{
public:
    // 输出目标是 `PkString*` 而不是 brief 原始签名的 `PkByteArray*`——已知偏离，
    // 与 Task 1 的 `PkXmlDocument::toByteArray()` 同一个决定（`PkByteArray`
    // 在本 worktree 尚未交付，`pk/container/` 下现场确认没有任何 *ByteArray*
    // 文件），不是重新判断一遍，见 pk/xml/README.md §2.2。
    explicit PkXmlStreamWriter(PkString *output);

    void setAutoFormatting(bool enable) { _autoFormatting = enable; }
    bool autoFormatting() const { return _autoFormatting; }

    void writeStartDocument();
    void writeEndDocument();
    void writeStartElement(const PkString &name);
    void writeEndElement();
    void writeAttribute(const PkString &name, const PkString &value);
    void writeCharacters(const PkString &text);

private:
    struct OpenElement {
        PkString name;
        // 探针 P10：只含文本、没有子元素的元素起止标签仍在同一行——闭合标签
        // 要不要在 autoFormatting 模式下换行+缩进，取决于这个元素内部有没有
        // 写过至少一个子*元素*（不是有没有写过文本）。
        bool hasChildElement = false;
    };

    void _closeStartTagIfOpen();
    void _sync();

    PkString *_output;
    std::string _buf; // UTF-8 累积缓冲区，每次公开方法结束时同步进 *_output
    bool _autoFormatting = false;
    bool _startTagOpen = false; // 最内层元素的起始标签 `<tag` 还没被 `>` 关闭
    std::vector<OpenElement> _openElements;
};
