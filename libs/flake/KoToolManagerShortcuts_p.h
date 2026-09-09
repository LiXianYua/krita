/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KO_TOOL_MANAGER_SHORTCUTS_P_H
#define KO_TOOL_MANAGER_SHORTCUTS_P_H

#include <PkList.h>

#include <QAction>

#include <utility>
#include <vector>

namespace KoToolManagerShortcuts
{

using EncodedShortcut = std::vector<int>;

// QAction is the retained host boundary. Copy its Qt shortcut payload into
// native encoded chords immediately; manager-side collision processing never
// stores or compares toolkit shortcut values.
inline PkList<EncodedShortcut> fromHostAction(const QAction &action)
{
    PkList<EncodedShortcut> result;
    for (const auto &hostShortcut : action.shortcuts()) {
        if (hostShortcut.toString().isEmpty()) {
            continue;
        }

        EncodedShortcut chords;
        chords.reserve(static_cast<std::size_t>(hostShortcut.count()));
        for (int i = 0; i < hostShortcut.count(); ++i) {
            chords.push_back(hostShortcut[i]);
        }
        result.append(std::move(chords));
    }
    return result;
}

}

#endif
