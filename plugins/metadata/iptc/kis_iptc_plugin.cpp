/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkStringHash.h>

#include <kis_meta_data_backend_registry.h>

#include "kis_iptc_io.h"

namespace
{
struct KisIptcBackendRegistration
{
    KisIptcBackendRegistration()
    {
        KisMetadataBackendRegistry::instance()->add(new KisIptcIO());
    }
};

KisIptcBackendRegistration s_kisIptcBackendRegistration;
} // namespace
