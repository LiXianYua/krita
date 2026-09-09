/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#ifndef KARBON_TOOLS_RESOURCES_H
#define KARBON_TOOLS_RESOURCES_H

#include <cstddef>

struct KarbonToolsResource
{
    const unsigned char *data;
    std::size_t size;
};

KarbonToolsResource karbonCalligraphyIconPng();
void karbonToolsResourceAnchor();

#endif
