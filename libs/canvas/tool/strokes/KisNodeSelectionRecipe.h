/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISNODESELECTIONRECIPE_H
#define KISNODESELECTIONRECIPE_H

#include <QHash>
#include <QPoint>

#include "kis_types.h"
#include <kritacanvas_export.h>


class KRITACANVAS_EXPORT KisNodeSelectionRecipe
{
public:
    enum SelectionMode {
        SelectedLayer,
        FirstLayer,
        Group
    };

    KisNodeSelectionRecipe(KisNodeList _selectedNodes);
    KisNodeSelectionRecipe(KisNodeList _selectedNodes, SelectionMode _mode, QPoint _pickPoint);
    KisNodeSelectionRecipe(const KisNodeSelectionRecipe &rhs) = default;
    KisNodeSelectionRecipe(const KisNodeSelectionRecipe &rhs, int levelOfDetail);

    KisNodeList selectNodesToProcess() const;

    KisNodeList selectedNodes;
    SelectionMode mode;
    QPoint pickPoint;
};

#endif // KISNODESELECTIONRECIPE_H
