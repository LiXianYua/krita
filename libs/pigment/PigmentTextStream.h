/*
    SPDX-FileCopyrightText: 2026 S-03-a
    SPDX-License-Identifier: LGPL-2.1-or-later

    PigmentTextStream —— 文本流读写的零 Qt 垫片（S 线剥 Qt 用），读写文本
    （GPL 调色板 / GGRA 渐变）。
    消费方：resources/KoColorSet.cpp（saveGpl/readAllLinesSafe）、
    resources/KoSegmentGradient.cpp（loadFromDevice/saveToDevice）、
    resources/KoStopGradient.cpp:630（saveToDevice）。

    构造时把 device 的剩余内容读进内部读缓冲（m_readBuf），readLine/readAll/>> 都
    在缓冲上操作；写侧（<<）进 m_writeBuf，flush()/析构时写给 device。内部统一
    UTF-8（Krita 消费方经 setUtf8OnStream 设 UTF-8；setCodec 为对齐 API 的 no-op）。

    相对 brief 的必要增补（消费方实测）：
      · operator<<(const char*)：saveGpl/saveToDevice 大量 `<< "GIMP Palette\n"`
        字符串字面量；
      · readLineInto(PkString&)：KoColorSet.cpp readAllLinesSafe 的
        `while (stream.readLineInto(&line))` 形态；
      · setAutoDetectUnicode(bool)：KoSegmentGradient.cpp:84 调用，no-op。

    ★ 本类与 `pk/textstream/PkTextStream.h` 的 `PkTextStream` **不是同一个类**、**不可互换**：
    本类构造时即把 device 剩余内容读进内部缓冲、读侧返回 `PkString`；那份是
    `std::string`/`FILE*`/`PkStream*` 多后端、读侧惰性。**权威实现在 `pk/textstream/`**（R-50）；
    本类是 S 线的过渡垫片，只为 `libs/pigment` 四个消费方存在（`git log` 见 S-03-a）。
 */

#ifndef PK_PIGMENT_TEXTSTREAM_H
#define PK_PIGMENT_TEXTSTREAM_H

#include <cstddef>

#include <PkStream.h>       // PkStream
#include <PkString.h>       // PkString
#include <PkAuxTypes.h>     // PkByteArray

class PigmentTextStream
{
public:
    explicit PigmentTextStream(PkStream *device);   // 读已有字节；构造时把剩余内容读进内部缓冲
    explicit PigmentTextStream(PkString *str);      // PkString 后端：写侧直接追加到 *str，读侧恒空（S-09-g SvgParser 调试流）
    ~PigmentTextStream();                           // 析构时 flush 写缓冲

    PkString readAll();
    PkString readLine();                       // 读一行，不含换行符
    bool readLineInto(PkString &line);         // 读到一行返回 true；EOF 返回 false
    bool atEnd() const;

    PigmentTextStream &operator>>(int &v);
    PigmentTextStream &operator>>(float &v);
    PigmentTextStream &operator>>(PkString &v);     // 空白分隔 token

    // 写侧（saveGpl/saveKpl/saveToDevice 需要）
    PigmentTextStream &operator<<(const PkString &s);
    PigmentTextStream &operator<<(const char *s);
    PigmentTextStream &operator<<(int v);
    PigmentTextStream &operator<<(qreal v);                 // 路径坐标写侧（setRealNumberPrecision 控制）
    void setRealNumberPrecision(int precision) { m_realPrec = precision; }
    PigmentTextStream &operator<<(char c);

    void flush();                              // 把内部写缓冲写给 device

    // 对齐 Qt 文本流的 codec/unicode 开关。本垫片内部统一 UTF-8，均为 no-op。
    void setCodec(const char *codecName);
    void setAutoDetectUnicode(bool enabled);

private:
    PkString *m_pkStr = nullptr;
    int m_realPrec = 6;                                // 对齐 QTextStream 默认 realNumberPrecision   // PkString 后端（写直通）
    PkStream *m_device;
    PkByteArray m_readBuf;
    std::size_t m_pos;
    PkByteArray m_writeBuf;

    void appendToWriteBuf(const char *data, std::size_t len);
    // 跳过空白；返回是否还有内容可读。
    bool skipWhitespace();
    // 从当前位置读一个空白分隔 token 到 out；返回是否读到内容。
    bool readToken(PkString &out);
};

#endif // PK_PIGMENT_TEXTSTREAM_H
