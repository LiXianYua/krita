/* This file is part of the KDE project
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

// KisConfigNotifier is lock-external and still carries Q_OBJECT/Q_SIGNALS
// declarations. Let the real QObject header define those host macros before
// parsing it, while keeping the mapper's state and connection Pk-native.
#include <QObject>

#include "kis_config_notifier.h"
#include "kis_selection_modifier_mapper.h"

void connectSelectionModifierMapperToConfigChanges(KisSelectionModifierMapper *mapper)
{
    PkObject::connect(KisConfigNotifier::instance(), &KisConfigNotifier::configChanged,
                      mapper, &KisSelectionModifierMapper::slotConfigChanged);
}
