/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkStringHash.h>

#include <kis_meta_data_backend_registry.h>

#include "kis_exif_io.h"

namespace
{
struct KisExifBackendRegistration
{
    KisExifBackendRegistration()
    {
        KisMetadataBackendRegistry::instance()->add(new KisExifIO());
    }
};

KisExifBackendRegistration s_kisExifBackendRegistration;
} // namespace
