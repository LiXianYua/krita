#pragma once

#include <string>
#include <vector>

#include "../container/PkArrayData.h"
#include "../container/PkByteArray.h"

// PkString —— 零 Qt 依赖的 COW UTF-16 字符串。
//
// 公开 API 的范围来自 docs/Qt替代品选型.md §2 的 QString 实测用量表，另含
// R-31 实测补入的 Unicode 默认大小写转换。
// 长度、下标、切片的单位一律是 **UTF-16 码元**，不是字素簇——与 QString 一致。
//
// 本头文件只写声明，实现全在 .cpp：验收脚本 replacement.sh ⑤ 会把头文件按
// `;{}` 切片后 grep `标识符(`，头文件里的内联函数体会被误算成「清单外的公开 API」。
class PkString
{
public:
    PkString();
    PkString(const char* utf8);          // 刻意不加 explicit：调用点靠 const char* 隐式转换
    PkString(char ch);                   // 单字节（ASCII）→ 单 UTF-16 码元
    PkString(char16_t ch);               // 单 UTF-16 码元
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
    // split(const char*)：对齐 QString::split(const char*) 的常见单 ASCII 字符分隔用法
    // （如 split("/")、split(",")），将 c 串首字符转 char16_t 后复用 split(char16_t)。
    std::vector<PkString> split(const char* sep) const
    {
        return split(static_cast<char16_t>(PkString(sep)[0]));
    }
    PkString toLower() const;
    PkString toUpper() const;

    // ── 用量表 · 格式化与转换 ────────────────────────────
    PkString& append(const PkString& other);
    PkString arg(const PkString& a) const;
    PkString arg(const PkString& a, const PkString& b) const;
    PkString arg(const PkString& a, const PkString& b, const PkString& c) const;   // 新增：R-13 补充
    PkString arg(int v) const;
    PkString arg(int v, int fieldWidth) const;   // 新增：Task 3 实现
    PkString arg(double v) const;
    int toInt(bool* ok = nullptr) const;
    double toDouble(bool* ok = nullptr) const;

    // ── 用量表 · 扩（flake 实测补入）────────────────────────
    // 以下方法原不在 QString 14 项用量表内。libs/flake 机械迁移后实测有真实用量
    // （toLatin1 127 / indexOf 82 / count 127 / isNull 69 / length 41 / remove 48
    // / replace 13 / toUtf8 27 / endsWith 15 / chop 7 / insert·simplified·setNum
    // ·compare 各若干），按「新发现的缺口直接补进用量表」原则补入，方法名与
    // QString 一致，flake 调用点无需改写即可编过。
    PkByteArray toLatin1() const;                 // 对齐 Qt：返回 PkByteArray（QByteArray 替代）
    PkByteArray toUtf8() const;                   // 与 PkToUtf8() 并存（后者返 std::string）
    int indexOf(const PkString& sub, int from = 0) const;
    bool endsWith(const PkString& suffix) const;
    void chop(int n);
    int count() const;                            // 同 size()
    int count(const PkString& sub) const;         // 子串出现次数
    bool isNull() const;                          // PkString 无 null 态：等价 isEmpty()
    int length() const;                           // 同 size()
    PkString simplified() const;                  // 去首尾空白并折叠内部空白为单空格
    int compare(const PkString& other) const;     // <0/0/>0，码元序
    PkString& replace(int pos, int n, const PkString& after);
    PkString& replace(const PkString& before, const PkString& after);
    PkString& replace(char16_t before, char16_t after);
    PkString& remove(int pos, int n);
    PkString& remove(const PkString& sub);
    PkString& insert(int pos, const PkString& s);
    PkString& insert(int pos, char16_t c);
    PkString& setNum(int n);
    PkString& setNum(int n, int base);
    PkString& setNum(double n);
    static PkString number(int n, int base = 10);
    static PkString number(double n);

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
    static PkString fromUtf8(const char* s);                       // 对齐 QString::fromUtf8
    static PkString fromUtf8(const char* s, int len)               // 定长重载（S-09-g KoFontGlyphModel）
    {
        return PkFromUtf8(s, len);
    }
    static PkString fromUtf8(const PkByteArray& ba)                // PkByteArray 字节（UTF-8 语义）
    {
        return PkFromUtf8(ba.data(), int(ba.size()));
    }
    // 对齐 QString::fromUcs4（Qt5 uint* 口径；hb_codepoint_t 即 uint32_t）。
    static PkString fromUcs4(const uint32_t* u, int len = -1)
    {
        std::u16string out;
        if (!u) return PkString();
        int n = len;
        if (n < 0) { n = 0; while (u[n]) ++n; }
        for (int i = 0; i < n; ++i) {
            uint32_t cp = u[i];
            if (cp >= 0x10000u) {
                cp -= 0x10000u;
                out.push_back(char16_t(0xD800 + (cp >> 10)));
                out.push_back(char16_t(0xDC00 + (cp & 0x3FF)));
            } else {
                out.push_back(char16_t(cp));
            }
        }
        return fromUtf16(out.data(), int(out.size()));
    }
    // 对齐 QVector<T>::join 的静态形式（std::vector 没法加成员；调用点写成
    // PkString::join(list, sep)——S-09-g 实测改写 3 处）。
    template <typename Range>
    static PkString join(const Range& list, const PkString& sep)
    {
        PkString out;
        bool first = true;
        for (const PkString& item : list) {
            if (!first) out += sep;
            out += item;
            first = false;
        }
        return out;
    }
    void clear() { _data().clear(); }
    // 对齐 QString::toUcs4：UTF-16 → UCS-4 码点序列（S-09-g KoFontGlyphModel）。
    std::vector<uint32_t> toUcs4() const
    {
        std::vector<uint32_t> out;
        const std::u16string u16 = PkToU16();
        for (std::size_t i = 0; i < u16.size(); ++i) {
            uint32_t c = static_cast<uint16_t>(u16[i]);
            if (c >= 0xD800u && c <= 0xDBFFu && i + 1 < u16.size()) {
                uint32_t lo = static_cast<uint16_t>(u16[i + 1]);
                if (lo >= 0xDC00u && lo <= 0xDFFFu) {
                    out.push_back(0x10000u + ((c - 0xD800u) << 10) + (lo - 0xDC00u));
                    ++i;
                    continue;
                }
            }
            out.push_back(c);
        }
        return out;
    }
    static PkString fromUtf16(const char16_t* s, int len = -1);    // 对齐 QString::fromUtf16
    bool PkIsSharedWith(const PkString& other) const;

private:
    // 只读访问，绝不 detach（PkArrayData::PkConst 的语义）。
    const std::vector<char16_t>& _cbuf() const;
    const char16_t* _cdata() const;
    // 写访问，返回前先 detach（PkArrayData::PkMut 的语义）。
    std::vector<char16_t>& _data();

    PkArrayData<std::vector<char16_t>> _d;
};

// 自由运算符：const char* + PkString（字面量在左）。PkString + const char* 走成员
// operator+(const PkString&) 经隐式 const char* 构造，无需此处。
inline PkString operator+(const char* a, const PkString& b)
{
    return PkString(a) + b;
}
