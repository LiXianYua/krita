#pragma once
#include "PkString.h"

// 编译期占位 —— i18n() 是 KDE 的 KLocalizedString 家族入口，本 worktree没有任何
// R 任务认领它的零 Qt 替代（国际化选型同 QLocale，属于未来未认领任务）。
//
// kis_dom_utils.cpp 里恰好两处调用：`findOnlyElement()`/`Private::checkType()`
// 各一处，都在"没找到对应 XML 元素"/"元素 type 属性不匹配"的错误分支里——
// kis_dom_utils_test.cpp 的 8 个测试方法全部是成功往返（save 完立刻 load 同一份
// 数据），从未进入这两个错误分支，因此这里只需要签名能编译、返回值能赋给
// `QString msg = i18n(...)`，不追求 KDE `%1`/`%2` 占位符替换的真实行为。
//
// 变长模板而不是固定 1~2 个 QString 重载：真实调用点用了 1 个和 2 个格式参数
// 两种形态（`i18n("...", tag, parent.tagName())`），变长模板一次覆盖，不用
// 分别声明。
template <typename... Args>
inline PkString i18n(const char *, Args &&...)
{
    return PkString();
}
