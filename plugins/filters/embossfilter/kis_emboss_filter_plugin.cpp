/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */


#include "kis_emboss_filter.h"
#include "kis_global.h"
#include "filter/kis_filter_registry.h"

namespace {
struct KisEmbossFilterPluginFilterRegistration
{
    KisEmbossFilterPluginFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisEmbossFilter());
    }
};
} // namespace
static KisEmbossFilterPluginFilterRegistration s_kisEmbossFilterPluginFilterRegistration;
