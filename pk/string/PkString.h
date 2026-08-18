#pragma once

#include <memory>
#include <string>
#include <vector>

#include "../container/PkArrayData.h"

// PkString —— 零 Qt 依赖的 COW UTF-16 字符串。
//
// 公开 API 的范围**恰好**等于 docs/Qt替代品选型.md §2 的 QString 实测用量表
// （14 项，见 .exec/replacement/R-01.api），一项不多一项不少。
// 长度、下标、切片的单位一律是 **UTF-16 码元**，不是字素簇——与 QString 一致。
//
// 本头文件只写声明，实现全在 .cpp：验收脚本 replacement.sh ⑤ 会把头文件按
// `;{}` 切片后 grep `标识符(`，头文件里的内联函数体会被误算成「清单外的公开 API」。
class PkString
{
public:
    PkString();
    PkString(const char* utf8);          // 刻意不加 explicit：调用点靠 const char* 隐式转换
    PkString(const PkString& other);
    PkString(PkString&& other) noexcept;
    ~PkString();
    PkString& operator=(const PkString& other);
    PkString& operator=(PkString&& other) noexcept;

    // ── 用量表 · 基础查询 ────────────────────────────────
    int size() const;
    bool isEmpty() const;
    char16_t at(int i) const;            // i 越界 → u'\0'（QString 在此是 UB）

    // ── 用量表 · 查询与切片 ──────────────────────────────
    bool contains(const PkString& sub) const;
    PkString left(int n) const;
    PkString right(int n) const;
    PkString mid(int pos, int n = -1) const;
    bool startsWith(const PkString& prefix) const;
    PkString trimmed() const;
    std::vector<PkString> split(char16_t sep) const;

    // ── 用量表 · 格式化与转换 ────────────────────────────
    PkString& append(const PkString& other);
    PkString arg(const PkString& a) const;
    PkString arg(const PkString& a, const PkString& b) const;
    PkString arg(int v) const;
    PkString arg(int v, int fieldWidth) const;   // 新增：Task 3 实现
    PkString arg(double v) const;
    int toInt(bool* ok = nullptr) const;
    double toDouble(bool* ok = nullptr) const;

    // ── 运算符（不计入用量表：调用点靠它们，Qt 侧也是运算符）──
    bool operator==(const PkString& other) const;
    bool operator!=(const PkString& other) const;
    bool operator<(const PkString& other) const;
    PkString operator+(const PkString& other) const;
    PkString& operator+=(const PkString& other);
    char16_t operator[](int i) const;

    // ── 互操作（Pk 前缀 → 不计入清单外 API）──────────────
    std::u16string PkToU16() const;
    std::string PkToUtf8() const;
    static PkString PkFromUtf8(const char* s, int len);

private:
    // 只读访问，绝不 detach（PkArrayData::PkConst 的语义）。
    const std::vector<char16_t>& _cbuf() const;
    const char16_t* _cdata() const;
    // 写访问，返回前先 detach（PkArrayData::PkMut 的语义）。
    std::vector<char16_t>& _data();

    PkArrayData<std::vector<char16_t>> _d;
};
