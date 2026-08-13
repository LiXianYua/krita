/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISDISPLAYCONFIGUIADAPTER_H
#define KISDISPLAYCONFIGUIADAPTER_H

#include "kritaui_export.h"
#include "canvas/KisDisplayConfig.h"

class KisConfig;

KRITAUI_EXPORT KisDisplayConfig::Options kisDisplayConfigOptionsFromKisConfig(const KisConfig &cfg);

#endif // KISDISPLAYCONFIGUIADAPTER_H
