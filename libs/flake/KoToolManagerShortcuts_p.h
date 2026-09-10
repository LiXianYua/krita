/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KO_TOOL_MANAGER_SHORTCUTS_P_H
#define KO_TOOL_MANAGER_SHORTCUTS_P_H

#include <vector>

namespace KoToolManagerShortcuts
{

// 桶无关的快捷键编码：一个 shortcut 是一串 chord，每个 chord 是若干 native key 的序列。
// 编码本身由宿主侧（KoCanvasController::hostActions()）产生，管理器只做比较，因此这里
// 不再有任何 Qt 类型。
using EncodedShortcut = std::vector<int>;

}

#endif
