#pragma once
#include <pk/string/PkString.h>

// 零 Qt 编译闭包内的 KLocalizedString 家族占位。
//
// 本文件在 `libs/flake/flake/noqt-compat/`（`-I` 位置 2，排在所有 `pk/<dir>` 之前），
// 是 flake 自有 compat 面里补这类缺失声明的天然落点——不动 `pk/`。
//
// 真实使用面（实测）：`libs/flake/shapes/ImageShapeFactory.cpp`（构造期两条
// `i18n("…")` 形状名/提示）与 `libs/flake/resources/KoSvgSymbolCollectionResource.cpp`
// （解析失败分支 `i18n("…", errorLine, errorColumn, toQString(errorMsg))`）。
// 两者都只要求签名可编译、返回值可参与 `toPkString(...)` / `operator<<`，
// 不追求 KDE `%1`/`%2` 占位符替换的真实行为——国际化选型尚未认领（同 QLocale）。
//
// 变长模板而非固定重载：真实调用点混用 1 个（仅格式串）与 3 个参数（int/int/PkString）
// 两种形态，变长模板一次覆盖。形态与既有先例
// `pk/xml/tests/graft/stubs/klocalizedstring.h` 一致，不另创。
template <typename... Args>
inline PkString i18n(const char *, Args &&...)
{
    return PkString();
}

/// 带上下文的兄弟形态。原生侧的使用面是 `KoPathTool.cpp` 的两条状态文本
/// （`i18nc("%1 is a shortcut to be pressed", "Press %1 to …", <shortcut>)`）。
/// 与 `i18n` 同款：只要求签名可编译、返回值可参与 `toPkString(...)`，
/// 不追求 KDE `%1` 占位符替换的真实行为。
template <typename... Args>
inline PkString i18nc(const char *, const char *, Args &&...)
{
    return PkString();
}
