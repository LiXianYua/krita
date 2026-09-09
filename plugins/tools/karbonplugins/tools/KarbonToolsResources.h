/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KARBON_TOOLS_RESOURCES_H
#define KARBON_TOOLS_RESOURCES_H

#include <cstddef>

struct KarbonToolsResource
{
    const char *iconName;
    const char *legacyPath;
    const unsigned char *data;
    std::size_t size;
};

using KarbonToolsResourceRegistrar = void (*)(const KarbonToolsResource &resource);

KarbonToolsResource karbonCalligraphyIconPng();
void setKarbonToolsResourceRegistrar(KarbonToolsResourceRegistrar registrar);
void registerKarbonToolsResources();

#endif
