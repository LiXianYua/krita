#pragma once

#include <cstddef>
#include <string>

// PkXmlOffsetUtil —— R-25 Task 3：offset→行列换算 + pugixml"起始位置"到 Qt
// "识别完位置"的换算，DOM 侧（PkXmlNode）与 Stream 侧（PkXmlStreamReader）
// 共用同一份逻辑，不各写一份——`PkXmlDocument::setContentImpl` 的
// errorLine/errorColumn 换算也复用这里的前两个函数（消重复代码，见该函数的
// 实现注释）。背景与探针实测见 docs/superpowers/plans/R-25.md「探针实测
// P15」「pugixml 侧 offset_debug() 对照实测」「P16」「设计②」。

// offset（0-based 字节偏移，指向 buf 里的某个具体字符）→ 1-based 行号/列号。
// 语义：col(offset) 是 buf 中处于 `offset` 位置的那个字符之前（不含它自己）、
// 当前行内已经出现过的字符数 + 1；行号从 1 开始，每遇到一个 '\n' 计一行。
// offset 越界（<0 或 >= buf.size()）时按"扫到 buf 末尾"处理，不越界访问、
// 不崩溃——调用方（PkXmlNode::lineNumber() 等）负责在语义上无效时短路返回
// -1，这两个函数本身只管字节到行列的机械换算。
int pkXmlOffsetToLine(const std::string &buf, std::ptrdiff_t offset);
int pkXmlOffsetToColumn(const std::string &buf, std::ptrdiff_t offset);

// 元素节点：pugixml `offset_debug()` 给的是标签名*起始*位置（紧跟 `<` 后面），
// 而 Qt `QDomNode::lineNumber()`/`columnNumber()` 报告的是这个元素自己的
// 开始标签*解析完*那一刻（`>` 或自闭合 `/` 本身的位置）——探针 P15/P16 已经
// 用真实 Qt 5.15.7 实测确认，两者不是一回事。本函数从 `nameStart` 向后扫，
// 跳过引号（单/双引号）包裹的属性值内容（处理属性值里出现字面 `>` 的情形，
// 那种 `>` 不算数），找到这个标签自己的闭合字符：普通标签落在 `>` 本身，
// 自闭合标签落在 `/` 本身（不是紧随其后的 `>`）——返回的是这个闭合字符的
// 0-based 偏移，直接喂给 `pkXmlOffsetToLine`/`pkXmlOffsetToColumn` 就能得到
// 与 Qt 一致的行列号。黄金数据（R-25.md 探针 P15/P16 逐字核对）：
//   `<root>`                → 闭合字符是 col=6 位置的 '>'
//   `<b x='1'/>`             → 闭合字符是 col=13 位置的 '/'（不是 col=14 的 '>'）
//   `<UNKNOWNROOT attr='1'>` → 闭合字符是 col=22 位置的 '>'
// `nameStart < 0`（未解析/无效）时原样透传返回，调用方负责在这之前短路。
// 扫到 buf 末尾都没找到闭合字符（畸形输入）时同样原样透传 nameStart——不是
// 精确值，但不崩溃、不死循环。
std::ptrdiff_t pkXmlAdjustElementOffsetToTagClose(const std::string &buf,
                                                   std::ptrdiff_t nameStart);

// 文本/CDATA 节点：Qt 报告的是"文本内容读完"那一刻（紧跟内容之后、下一个
// token 的起点），不需要扫描 buf——直接是内容起始偏移加内容的字节长度。
// 探针 P15 的"text node: line=2 col=6"黄金数据（输入含真实换行的文本节点）
// 用这个公式精确复现。
std::ptrdiff_t pkXmlAdjustTextOffsetToContentEnd(std::ptrdiff_t contentStart,
                                                  std::size_t contentByteLen);
