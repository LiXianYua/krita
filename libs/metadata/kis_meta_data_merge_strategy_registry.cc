/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <PkStringHash.h>

#include "kis_debug.h"
#include "kis_meta_data_merge_strategy_registry.h"
#include "kis_meta_data_merge_strategy_p.h"

using namespace KisMetaData;

MergeStrategyRegistry::MergeStrategyRegistry()
{
    add(new DropMergeStrategy());
    add(new PriorityToFirstMergeStrategy());
    add(new OnlyIdenticalMergeStrategy());
    add(new SmartMergeStrategy());
}

MergeStrategyRegistry::MergeStrategyRegistry(const MergeStrategyRegistry&) : KoGenericRegistry<const KisMetaData::MergeStrategy*>()
{
}

MergeStrategyRegistry& MergeStrategyRegistry::operator=(const MergeStrategyRegistry&)
{
    return *this;
}

MergeStrategyRegistry::~MergeStrategyRegistry()
{
    for (const PkString &id : keys()) {
        delete get(id);
    }
    dbgRegistry << "Deleting MergeStrategyRegistry";
}

MergeStrategyRegistry* MergeStrategyRegistry::instance()
{
    static MergeStrategyRegistry s_instance;
    return &s_instance;
}

