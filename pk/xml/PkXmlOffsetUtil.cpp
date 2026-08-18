#include "PkXmlOffsetUtil.h"

#include <algorithm>

namespace {

// 一次扫描同时算出行与列——PkXmlDocument::setContentImpl 重构前的原始循环
// 就是这个形态（两个变量一起累加），拆成 pkXmlOffsetToLine/pkXmlOffsetToColumn
// 两个公开函数只是为了给 DOM/Stream 两侧一个更小的调用面，内部仍然共享同一份
// 扫描逻辑，避免出现两份"数 \n"的代码。
void pkXmlScanLineCol(const std::string &buf, std::ptrdiff_t offset, int &line, int &col)
{
    line = 1;
    col = 1;
    const std::ptrdiff_t limit =
        std::min(offset, static_cast<std::ptrdiff_t>(buf.size()));
    for (std::ptrdiff_t i = 0; i < limit; ++i) {
        if (buf[static_cast<std::size_t>(i)] == '\n') {
            ++line;
            col = 1;
        } else {
            ++col;
        }
    }
}

} // namespace

int pkXmlOffsetToLine(const std::string &buf, std::ptrdiff_t offset)
{
    int line = 1;
    int col = 1;
    pkXmlScanLineCol(buf, offset, line, col);
    return line;
}

int pkXmlOffsetToColumn(const std::string &buf, std::ptrdiff_t offset)
{
    int line = 1;
    int col = 1;
    pkXmlScanLineCol(buf, offset, line, col);
    return col;
}

std::ptrdiff_t pkXmlAdjustElementOffsetToTagClose(const std::string &buf, std::ptrdiff_t nameStart)
{
    if (nameStart < 0) {
        return nameStart;
    }
    char quote = '\0';
    const std::ptrdiff_t size = static_cast<std::ptrdiff_t>(buf.size());
    for (std::ptrdiff_t i = nameStart; i < size; ++i) {
        const char c = buf[static_cast<std::size_t>(i)];
        if (quote != '\0') {
            // 引号内的任何字符（含字面 '>'）都不算闭合标记，只找与开引号
            // 配对的那一个引号来退出引号状态。
            if (c == quote) {
                quote = '\0';
            }
            continue;
        }
        if (c == '\'' || c == '"') {
            quote = c;
            continue;
        }
        if (c == '>') {
            if (i > nameStart && buf[static_cast<std::size_t>(i - 1)] == '/') {
                return i - 1; // 自闭合标签：落在 '/' 本身，不是紧随其后的 '>'。
            }
            return i;
        }
    }
    // 扫到 buf 末尾都没找到闭合字符：畸形输入，原样返回起始位置，换算出的
    // 行列不精确但不越界、不崩溃。
    return nameStart;
}

std::ptrdiff_t pkXmlAdjustTextOffsetToContentEnd(std::ptrdiff_t contentStart, std::size_t contentByteLen)
{
    return contentStart + static_cast<std::ptrdiff_t>(contentByteLen);
}
