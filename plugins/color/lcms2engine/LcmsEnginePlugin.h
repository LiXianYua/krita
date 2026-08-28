/*
 *  SPDX-FileCopyrightText: 2003 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/
#ifndef KO_LCMS_ENGINE_PLUGIN_H
#define KO_LCMS_ENGINE_PLUGIN_H

/** Register the LCMS engine, profiles, color spaces, and histogram producers.
 *
 * The function is idempotent so the local MODULE initializer and focused tests
 * can share the same registration entry point without creating duplicate
 * registry entries.
 */
void registerLcmsEngine();

#endif // KO_LCMS_ENGINE_PLUGIN_H
