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

/**
 * Installs the process-lifetime resource registrar during externally serialized
 * host initialization, before any call to registerKarbonToolsResources() or
 * registerKarbonTools().  The registrar function and any state it uses must
 * remain valid until process exit; replacement and unregistration are not
 * supported.
 *
 * Returns true only for the first non-null registrar installed before resource
 * registration starts.  Null, duplicate, and late installations return false
 * without changing the installed registrar.  If registration starts without a
 * registrar, resource delivery is permanently skipped for this process.
 *
 * The callback is invoked synchronously at most once, even when registration is
 * requested repeatedly or concurrently.  Its KarbonToolsResource argument is a
 * temporary descriptor and must be copied if retained.  The descriptor's
 * identity strings and data bytes have static storage and remain valid until
 * process exit.
 */
bool setKarbonToolsResourceRegistrar(KarbonToolsResourceRegistrar registrar);

/**
 * Delivers the embedded resources according to the installation contract above.
 * Repeated and concurrent calls are idempotent.
 */
void registerKarbonToolsResources();

#endif
