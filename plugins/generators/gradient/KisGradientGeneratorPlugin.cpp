/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2020 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <generator/kis_generator_registry.h>

#include "KisGradientGenerator.h"
#include "KisGradientGeneratorPlugin.h"

namespace { const bool registered = [] {
    KisGeneratorRegistry::instance()->add(new KisGradientGenerator());
    return true;
}(); }
