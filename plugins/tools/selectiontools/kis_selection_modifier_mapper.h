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
#include <PkNamespace.h>
#include <PkObject.h>
#include <PkScopedPointer.h>
#include <type_traits>

class KisSelectionModifierMapper : public PkObject
{
public:
    KisSelectionModifierMapper();
    ~KisSelectionModifierMapper() override;
    static KisSelectionModifierMapper *instance();
    static SelectionAction map(Pk::KeyboardModifiers m);
    template <typename HostModifiers,
              typename = std::enable_if_t<!std::is_same_v<std::decay_t<HostModifiers>,
                                                        Pk::KeyboardModifiers>>>
    static SelectionAction map(HostModifiers m)
    {
        return map(Pk::KeyboardModifiers(PkFlag(static_cast<int>(m))));
    }

public:
    void slotConfigChanged();

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif
