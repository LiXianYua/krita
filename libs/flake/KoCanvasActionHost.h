/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Host action identity contract between retained tool managers and the UI host.
 */
#ifndef KOCANVASACTIONHOST_H
#define KOCANVASACTIONHOST_H

#include <PkList.h>
#include <PkString.h>
#include <PkStringList.h>
#include <PkNamespace.h>

#include <vector>

#include "kritaflake_export.h"

/** 宿主 action 标识 —— 工厂对宿主动作集合的声明。 */
struct KRITAFLAKE_EXPORT KisHostActionSpec
{
    const char *text = nullptr;                    ///< 未翻译源文本，翻译在宿主边界发生
    PkString objectName;                           ///< 动作标识
    Pk::Key shortcut = static_cast<Pk::Key>(0);    ///< 0 = 无默认快捷键
};

/** 宿主 action 标识 —— 宿主对管理器的回报。 */
struct KRITAFLAKE_EXPORT KisHostActionIdentity
{
    PkString objectName;
    bool carriesToolAction = false;                ///< 宿主侧 property("tool_action") 有效
    PkStringList toolIds;                          ///< 该 property 的值
    bool alwaysEnabled = false;                    ///< property("always_enabled")
    PkList<std::vector<int>> shortcutChords;       ///< encoded chord，空 chord 已丢弃
};

class KRITAFLAKE_EXPORT KoCanvasActionHost
{
public:
    virtual ~KoCanvasActionHost() = default;
    virtual PkList<KisHostActionIdentity> hostActions() const { return {}; }
    virtual void setHostActionEnabled(const PkString &objectName, bool enabled)
    { (void)objectName; (void)enabled; }
};

#endif // KOCANVASACTIONHOST_H
