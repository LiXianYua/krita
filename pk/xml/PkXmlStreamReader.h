#pragma once

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

    bool _started = false; // 第一次 readNext() 之前
    bool _atEnd = false;
    TokenType _tokenType = NoToken;
    PkString _name;
    PkString _text;
    PkXmlStreamAttributes _attributes;
    std::vector<Frame> _stack;
};
