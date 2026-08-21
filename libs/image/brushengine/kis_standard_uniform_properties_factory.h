/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_STANDARD_UNIFORM_PROPERTIES_FACTORY_H
#define __KIS_STANDARD_UNIFORM_PROPERTIES_FACTORY_H

#include <KoID.h>
#include <PkString.h>

#include "kis_uniform_paintop_property.h"

class KisPaintOpPresetUpdateProxy;

namespace KisStandardUniformPropertiesFactory
{
static const KoID size("size", PkString("Size"));
static const KoID opacity("opacity", PkString("Opacity"));
static const KoID flow("flow", PkString("Flow"));
static const KoID angle("angle", PkString("Angle"));
static const KoID spacing("spacing", PkString("Spacing"));


/**
     * Overload of createProperty(const PkString &id, ...)
     */
KisUniformPaintOpPropertySP createProperty(const KoID &id,
                                           KisPaintOpSettingsRestrictedSP settings,
                                           KisPaintOpPresetUpdateProxy *updateProxy);

/**
     * Factory for creating standard uniform properties. Right now
     * it supports only size, opacity and flow.
     */
KisUniformPaintOpPropertySP createProperty(const PkString &id,
                                           KisPaintOpSettingsRestrictedSP settings,
                                           KisPaintOpPresetUpdateProxy *updateProxy);
}

#endif /* __KIS_STANDARD_UNIFORM_PROPERTIES_FACTORY_H */
