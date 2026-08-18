#pragma once

#include <cstddef>
#include <string>
#include <vector>

#include <pugixml.hpp>

#include "../string/PkString.h"
#include "PkXmlStreamAttributes.h"

// PkXmlStreamReader —— QXmlStreamReader 的零 Qt 对应物。
//
// "预解析整树 + 游标遍历" 模拟 token 流（线级 plan Task2「关键实现说明 2」）：
// 构造时把整个输入用 pugixml 解析成一棵 `pugi::xml_document`（不消费 Task 1
// 的 PkXmlDocument/PkXmlNode 类型——README 已声明 Task 2 只复用 pugixml 这个
// 库本身，见 pk/xml/README.md 顶部），`readNext()` 内部维护一个显式栈做前序
// 遍历，每次调用前进一步，把当前访问到的节点映射成对应的 TokenType。这不是
// 真流式（整个输入必须先能装进内存解析成树），这是本任务已声明的范围决策，
// 不是遗漏——真实调用点（用量表 §1）全部是可以整体入内存的场景。
//
// TokenType 数值与探针 P11 实测的 Qt 枚举值逐一对齐，不能重排——真实调用点
// 用量表里可能直接比较数值或依赖 switch 的隐式顺序。Comment=7/DTD=8/
// EntityReference=9/ProcessingInstruction=10 真实调用点没有出现，不声明。
//
// 不可拷贝/不可移动：`_doc` 是本类唯一持有的 pugi::xml_document 值（不像
// PkXmlNode 家族那样用 shared_ptr 共享——本类只有自己一个所有者，没有多份
// 句柄需要共享同一棵树的场景）。移动/拷贝 xml_document 会让内部指针失效
// （见 PkXmlNode.h 顶部注释同一条坑），真实调用点也从不拷贝/移动
// QXmlStreamReader 实例（典型写法是 `QXmlStreamReader xml(data); xml.readNext();
// …` 原地使用），删掉比防御性地正确实现移动语义更省，也更安全。
class PkXmlStreamReader
{
public:
    enum TokenType {
        NoToken = 0,
        Invalid = 1,
        StartDocument = 2,
        EndDocument = 3,
        StartElement = 4,
        EndElement = 5,
        Characters = 6
    };

    explicit PkXmlStreamReader(const PkString &data);
    PkXmlStreamReader(const PkXmlStreamReader &) = delete;
    PkXmlStreamReader &operator=(const PkXmlStreamReader &) = delete;
    PkXmlStreamReader(PkXmlStreamReader &&) = delete;
    PkXmlStreamReader &operator=(PkXmlStreamReader &&) = delete;

    TokenType readNext();
    // R-07 全分支终审 C1 新增：真实调用点用量表原先把这两个方法记成
    // "实测 0 调用点，不实现"是漏统计（计数表达式漏了 `->` 接收者形态）——
    // `libs/pigment/resources/KoColorSet.cpp` 实测 readNextStartElement 4 处、
    // skipCurrentElement 3 处，见 pk/xml/README.md §7。用现有 readNext() 循环
    // 实现，语义与 Qt 对齐：readNextStartElement() 跳过 Characters/
    // StartDocument 一路 readNext()，遇到 StartElement 返回 true，遇到
    // EndElement/EndDocument/Invalid 返回 false；skipCurrentElement() 假定
    // 当前 tokenType() 是 StartElement（真实调用点都是这个前提），readNext()
    // 到它自己配对的 EndElement 为止（用一个深度计数器处理嵌套子元素）。
    bool readNextStartElement();
    void skipCurrentElement();
    bool atEnd() const { return _atEnd; }
    bool hasError() const { return _hasError; }
    PkString errorString() const { return _errorString; }
    // 调用点主动把 reader 置入错误状态（Qt 语义）——**不在 brief 的
    // Interfaces 枚举里**，是实现前对真实调用点用量复核发现的缺口：
    // `libs/pigment/resources/KoColorSet.cpp` 有 6 处 `xml->raiseError(...)`
    // （线级 plan 用量表把它记成「实测 0 调用点」，是漏统计，不是真的没有）。
    // 按 CLAUDE.md「新发现的缺口直接补进来」原则补上，见 PkXmlStreamReader.cpp
    // 的实现注释。
    void raiseError(const PkString &message);

    TokenType tokenType() const { return _tokenType; }
    PkString name() const { return _name; }
    PkString text() const { return _text; }
    PkXmlStreamAttributes attributes() const { return _attributes; }

    // R-25 Task 3：QXmlStreamReader::lineNumber()/columnNumber() 的零 Qt
    // 对应物。两个场景精确对齐真实调用点用法（探针 P16，
    // docs/superpowers/plans/R-25.md）：①构造期解析失败——复用构造函数已经
    // 算过的 `result.offset`；②`readNextStartElement()` 找到 StartElement
    // 后立即查——取当前 Frame::element 的 offset_debug()，跟 DOM 侧
    // `PkXmlNode::lineNumber()` 共用同一个"扫到标签闭合处"算法
    // （PkXmlOffsetUtil.h）。其余 tokenType 下按同一算法尽力而为，不专门
    // 验证——真实调用点只用到这两种场景。
    int lineNumber() const;
    int columnNumber() const;

private:
    struct Frame {
        pugi::xml_node element;
        pugi::xml_node cursor; // 下一个待处理的子节点；未开始时等于 element.first_child()
        bool entered = false;  // element 自己的 StartElement 是否已经发出过
    };

    static PkXmlStreamAttributes _collectAttributes(const pugi::xml_node &element);

    pugi::xml_document _doc;
    pugi::xml_node _root; // 文档根元素（parse 失败或空文档时为空句柄）
    bool _parsedOk = false;
    bool _hasError = false;
    PkString _errorString;

    // R-25 Task 3：构造函数里解析用的 utf8 字节，原来是局部变量，改存成员——
    // lineNumber()/columnNumber() 的 offset→行列换算需要访问原始字节，跟
    // PkXmlDocument::_doc->source 是同一个道理（PkXmlNode.h 顶部 PkXmlDocRoot
    // 注释）。
    std::string _sourceUtf8;
    // 构造期解析失败时 pugixml 报告的字节偏移（`result.offset`）——只在那个
    // 分支里赋值，之后终生不变（构造失败是冻结状态，见类头部"不可拷贝/不可
    // 移动"注释旁的语义约定）。
    std::ptrdiff_t _errorOffset = -1;
    bool _constructionFailed = false;

    bool _started = false; // 第一次 readNext() 之前
    bool _atEnd = false;
    TokenType _tokenType = NoToken;
    PkString _name;
    PkString _text;
    PkXmlStreamAttributes _attributes;
    std::vector<Frame> _stack;
};
