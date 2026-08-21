/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *  SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_meta_data_backend_registry.h"

#include <PkStringHash.h>

#include <kis_debug.h>

KisMetadataBackendRegistry::KisMetadataBackendRegistry()
{
}

KisMetadataBackendRegistry::~KisMetadataBackendRegistry()
{
    for (const PkString &id : keys()) {
        delete get(id);
    }
    dbgRegistry << "Deleting KisMetadataBackendRegistry";
}

void KisMetadataBackendRegistry::init()
{
    // D-12：静态注册由 S-09-c 的 registerAllPlugins() 调用 instance()->add() 完成。
    // 这里保留为空，不再动态加载 "Krita/Metadata" 插件。
}

KisMetadataBackendRegistry *KisMetadataBackendRegistry::instance()
{
    static KisMetadataBackendRegistry s_instance;
    return &s_instance;
}
