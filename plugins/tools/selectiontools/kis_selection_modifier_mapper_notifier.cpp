/* This file is part of the KDE project
 * SPDX-License-Identifier: LGPL-3.0-or-later
 */

#include "kis_config_notifier.h"
#include "kis_selection_modifier_mapper.h"

void connectSelectionModifierMapperToConfigChanges(KisSelectionModifierMapper *mapper)
{
    PkObject::connect(KisConfigNotifier::instance(), &KisConfigNotifier::configChanged,
                      mapper, &KisSelectionModifierMapper::slotConfigChanged);
}
