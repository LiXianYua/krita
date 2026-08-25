/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_transform_mask_params_factory_registry.h"


#include "kis_transform_mask_params_interface.h"

static KisTransformMaskParamsFactoryRegistry *s_instance()
{
    static KisTransformMaskParamsFactoryRegistry instance;
    return &instance;
}


KisTransformMaskParamsFactoryRegistry::KisTransformMaskParamsFactoryRegistry()
{
}

KisTransformMaskParamsFactoryRegistry::~KisTransformMaskParamsFactoryRegistry()
{
}

void KisTransformMaskParamsFactoryRegistry::addFactory(const PkString &id, const KisTransformMaskParamsFactory &factory)
{
    m_map.insert(id, factory);
}

KisTransformMaskParamsInterfaceSP
KisTransformMaskParamsFactoryRegistry::createParams(const PkString &id, const PkXmlElement &e)
{
    KisTransformMaskParamsFactoryMap::iterator it = m_map.find(id);
    return it != m_map.end() ? (*it)(e) : KisTransformMaskParamsInterfaceSP(0);
}

void KisTransformMaskParamsFactoryRegistry::setAnimatedParamsHolderFactory(const KisAnimatedTransformMaskParamsHolderFactory &factory)
{
    m_animatedParamsFactory = factory;
}

KisAnimatedTransformParamsHolderInterfaceSP KisTransformMaskParamsFactoryRegistry::createAnimatedParamsHolder(KisDefaultBoundsBaseSP defaultBounds)
{
    KIS_ASSERT(m_animatedParamsFactory);
    return m_animatedParamsFactory(defaultBounds);
}

KisTransformMaskParamsFactoryRegistry*
KisTransformMaskParamsFactoryRegistry::instance()
{
    return s_instance();
}
