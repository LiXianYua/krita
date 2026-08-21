/*
 *  SPDX-FileCopyrightText: 2026 S-08 (R-31)
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOLCLOCALE_H
#define KOLCLOCALE_H

#include <pk/string/PkString.h>

// S-08 R-31：locale 感知 casing 的独立 locale 层（QLocale 替代品）。
//
// 真 Qt 5.15.7 行为基准见 .superpowers/sdd/S-08/locale/probe-findings.md，
// oracle 对拍见 .superpowers/sdd/S-08/locale/oracle/（R-31.deviation 登记差异）。
// 与 PkString::toLower/toUpper（Unicode 13.0 默认规则，无 locale 参数）的区别：
// 本层做 locale 特例——Turkish/Azeri 的 İ/ı、Dutch 的 IJ 大写化、以及 bcp47Name
// 的语言标签规范化。德语 ß→SS 已含在 PkString 默认大写（Unicode 无条件
// SpecialCasing），无需特例。
namespace KoLc {

// langCode 形如 "en"/"tr"/"nl"/"zh-Hans-CN"，内部统一归一化 "-"/"_"。
PkString toUpper(const PkString &text, const PkString &langCode);
PkString toLower(const PkString &text, const PkString &langCode);

// 按词首 grapheme 大写（CSS text-transform: capitalize 的 locale 层核心）。
// Dutch：词首 "ij" 两个字母都大写（"ijsbeer" → "IJsbeer"）。
PkString capitalize(const PkString &text, const PkString &langCode);

// 语言标签规范化（Qt QLocale::bcp47Name() 的 CLDR likely-subtags 语义，
// 保留冗余子标签的最短形式）。
PkString bcp47Name(const PkString &langCode);

// Dutch 判定（KoCssTextUtils.cpp 的 IJ 特殊分支用）。
bool isDutch(const PkString &langCode);

} // namespace KoLc

#endif // KOLCLOCALE_H
