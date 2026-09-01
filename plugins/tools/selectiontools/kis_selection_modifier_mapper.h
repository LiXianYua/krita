/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2016 Michael Abrahams <miabraha@gmail.com>
 *
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#ifndef KIS_SELECTION_MODIFIER_MAPPER_H_
#define KIS_SELECTION_MODIFIER_MAPPER_H_

/**
 * See KisToolSelectBase for usage.
 */

#include "kis_selection.h"
#include <PkObject.h>
#include <PkScopedPointer.h>

class KisSelectionModifierMapper : public PkObject
{
public:
    KisSelectionModifierMapper();
    ~KisSelectionModifierMapper() override;
    static KisSelectionModifierMapper *instance();
    static SelectionAction map(Qt::KeyboardModifiers m);

public:
    void slotConfigChanged();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif
