#pragma once

#include <cstddef>
#include <string>
#include <vector>

// UTF-8 <-> UTF-16 编解码。原来挂在 PkStringData 的静态方法上，随 R-13 的 COW
// 地基迁移（PkStringData -> PkArrayData<vector<char16_t>>）拆成不依赖任何 COW
// 状态的自由函数——这两个函数本来就是纯函数，不需要类。
namespace PkStringCodec {

// 手写 UTF-8 -> UTF-16 解码。非法序列一律映射成 U+FFFD 并前进一个字节。
std::vector<char16_t> FromUtf8(const char* s, std::size_t len);

// UTF-16 -> UTF-8。孤立代理项（未配对的高/低代理码元）编码成单字节 0x3F（'?'），
// 与真实 Qt 5.15.7 的 QString::toUtf8() 逐位一致（探针见 R-13 plan 背景 ⑧）——
// **不是** U+FFFD 的三字节 UTF-8 编码，这是本任务要对齐的第 7 条根因。
std::string ToUtf8(const std::vector<char16_t>& b);

} // namespace PkStringCodec
