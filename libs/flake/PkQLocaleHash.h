#pragma once

// pkHash(const QLocale &) —— QLocale 作 PkHash 键时 PkHasher（pk/container）的
// ADL 扩展点。QLocale 无 Pk 等价物（过渡期真 Qt），Qt 自带 qHash(QLocale)，转发即可。
//
// 独立成头的理由：PkHash<QLocale, PkString> 的实例化点散在 KoSvgText.h 及其
// 消费 TU（text 命令、KoFontFamily、KoFFWWSConverter），这些 TU 不一定 include
// PkFlakeBridge.h——定义必须住在它们公共可见的头里（S-09-g 编译实测）。

#include <QLocale>
#include <QtGlobal>

inline unsigned int pkHash(const QLocale &key, unsigned int seed = 0) noexcept
{
    return qHash(key, seed);
}
