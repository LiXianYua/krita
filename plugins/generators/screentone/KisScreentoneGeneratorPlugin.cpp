/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2021 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <generator/kis_generator_registry.h>

#include "KisScreentoneGenerator.h"

namespace { const bool screentoneGeneratorRegistered = [] {
    KisGeneratorRegistry::instance()->add(new KisScreentoneGenerator());
    return true;
}(); }
