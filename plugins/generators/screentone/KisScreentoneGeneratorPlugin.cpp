/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2021 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <generator/kis_generator_registry.h>

#include "KisScreentoneGenerator.h"
#include "KisScreentoneGeneratorPlugin.h"

namespace { const bool registered = [] {
    KisGeneratorRegistry::instance()->add(new KisScreentoneGenerator());
    return true;
}(); }
