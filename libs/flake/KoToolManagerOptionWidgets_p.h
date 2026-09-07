/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KO_TOOL_MANAGER_OPTION_WIDGETS_P_H
#define KO_TOOL_MANAGER_OPTION_WIDGETS_P_H

#include <PkList.h>

#include <QList>
#include <QPointer>
#include <QWidget>

namespace KoToolManagerOptionWidgets
{

// QWidget remains a literal host identity. Convert to QPointer only at the
// KoToolManager delivery boundary so the host keeps teardown tracking.
inline QList<QPointer<QWidget>> toHostPointers(
    const PkList<QPointer<QWidget>> &widgets)
{
    QList<QPointer<QWidget>> result;
    for (const QPointer<QWidget> &widget : widgets) {
        result.append(widget);
    }
    return result;
}

}

#endif
