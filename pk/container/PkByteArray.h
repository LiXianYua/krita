#pragma once

#include <cstdint>
#include <vector>

// ---------------------------------------------------------------------------
// PkByteArray —— QByteArray 的零 Qt 替代。
// API 面与语义对齐 Qt 5.15（resize/number/data/constData 均由探针实测钉住，
// 见 pk/variant/oracle/ 下的对拍 ba_oracle）。
// ---------------------------------------------------------------------------
class PkByteArray
{
public:
    PkByteArray();
    PkByteArray(const char* data, int len);          // len<=0 按空处理（Qt 对 (char*,0) 合法）
    explicit PkByteArray(const std::vector<uint8_t>& data);

    // 对齐 QByteArray：data() 有可变/const 两个重载，constData() 恒 const。
    char*        data();                              // 可变
    const char*  data() const;                        // 空时返回非空 NUL 指针
    const char*  constData() const;                   // 空时返回非空 NUL 指针（探针）
    int          size() const;
    bool         isEmpty() const;

    // 对齐 QByteArray::resize（探针实测语义）：
    //   n<=0 → 清空（size 0）；n>size → 尾部补 0；n<size → 截断保留前缀。
    void         resize(int n);

    // 对齐 QByteArray::number（探针实测语义）：
    //   number(int, base)：base==10 带符号十进制；base==2/8/16 把 int 当 uint32
    //   打全 32 位补码，小写。
    //   number(uint, base)：base==10 无符号十进制；base==2/8/16 直接无符号，小写。
    static PkByteArray number(int n, int base = 10);
    static PkByteArray number(unsigned int n, int base = 10);

    bool operator==(const PkByteArray& other) const;
    bool operator!=(const PkByteArray& other) const;

private:
    std::vector<uint8_t> m_data;
};
