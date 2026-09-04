#ifndef PK_TEXTSTREAM_H
#define PK_TEXTSTREAM_H

// PkTextStream —— QTextStream 的零 Qt 替代（R-50）。
//
// 覆盖 text/svg 实测用量（Qt替代品选型.md §2 + libs/flake 调用点扫描）：
//   - SvgWriter/HtmlWriter：QTextStream(&outputDevice)  → 本类接 PkStream*（pk/port）
//   - SvgParser/SvgStyleWriter：QTextStream(&string)      → 本类接 std::string*
//   - 另有少量 FILE* 场景（测试/导出）
// 内部按"谁被设置就写到谁"分派；write 走 PkStream::write / std::string::append /
// fputs，read 走 PkStream::read/readLine / 缓冲 / fgetc。当前不做 locale/codec
// 全量对齐（setCodec 为占位）。

#include <string>
#include <cstdio>

class PkStream;   // pk/port，构造子用到；具体定义在 .cpp 里 include

class PkTextStream {
public:
    enum Status { Ok, ReadPastEnd, ReadCorruptData, WriteFailed };
    enum FieldAlignment { AlignLeft, AlignRight, AlignCenter, AlignAccountingStyle };
    enum NumberFlag { ShowBase = 0x1, ForcePoint = 0x2, ForceSign = 0x4,
                      UppercaseBase = 0x8, UppercaseDigits = 0x10 };

    explicit PkTextStream(std::string *str) : m_str(str) {}
    explicit PkTextStream(FILE *f) : m_file(f) {}
    explicit PkTextStream(PkStream *dev) : m_dev(dev) {}
    ~PkTextStream() {}

    // —— 写 ——
    PkTextStream &operator<<(const char *s);
    PkTextStream &operator<<(const std::string &s);
    PkTextStream &operator<<(char c);
    PkTextStream &operator<<(int v);
    PkTextStream &operator<<(long v);
    PkTextStream &operator<<(unsigned v);
    PkTextStream &operator<<(double v);
    PkTextStream &operator<<(float v);

    // —— 读 ——
    PkTextStream &operator>>(std::string &s);
    PkTextStream &operator>>(int &v);
    PkTextStream &operator>>(double &v);

    std::string readLine();
    std::string readAll();
    bool atEnd() const;
    Status status() const { return Ok; }
    void setStatus(Status) {}
    void seek(int) {}
    int pos() const { return 0; }

    void setCodec(const char *) {}            // 后续接 PkTextCodec
    void setRealNumberPrecision(int p) { m_prec = p; }
    void setFieldWidth(int w) { m_fieldWidth = w; }
    void setPadChar(char) {}
    void setFieldAlignment(FieldAlignment) {}
    void setNumberFlags(NumberFlag) {}
    void setIntegerBase(int) {}

    void flush();

private:
    std::string formatReal(double v);
    std::string *m_str = nullptr;
    FILE *m_file = nullptr;
    PkStream *m_dev = nullptr;
    int m_prec = 6;
    int m_fieldWidth = 0;
};

#endif // PK_TEXTSTREAM_H
