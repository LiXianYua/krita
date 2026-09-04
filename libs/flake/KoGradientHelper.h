/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2008 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KO_GRADIENT_HELPER_H
#define KO_GRADIENT_HELPER_H

#include <kritaflake_export.h>

#include <PkGradient.h>

namespace KoGradientHelper
{
/// creates default gradient
KRITAFLAKE_EXPORT PkGradient *defaultGradient(PkGradient::Type type, PkGradient::Spread spread, const PkGradientStops &stops);

/// Converts gradient type, preserving as much data as possible
KRITAFLAKE_EXPORT PkGradient *convertGradient(const PkGradient *gradient, PkGradient::Type newType);

/// Calculates color at given position from given gradient stops
KRITAFLAKE_EXPORT PkColor colorAt(qreal position, const PkGradientStops &stops);
}

#endif
