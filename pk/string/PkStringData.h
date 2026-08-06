#pragma once

#include <cstddef>
#include <memory>
#include <string>
#include <vector>

// COW 缓冲区。PkString 的所有实例共享一个 PkStringData，写时按引用计数分裂。
// 名字一律 Pk 前缀：它不是 QString 用量表里的公开 API，只是地基。
class PkStringData
{
public:
    std::vector<char16_t> buf;

    static std::shared_ptr<PkStringData> PkMakeEmpty();
    static std::shared_ptr<PkStringData> PkFromUtf8(const char* s, std::size_t len);
    static std::shared_ptr<PkStringData> PkClone(const PkStringData& other);
    static std::string PkToUtf8(const std::vector<char16_t>& b);
};
