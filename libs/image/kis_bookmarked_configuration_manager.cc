/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_bookmarked_configuration_manager.h"

#include <PkConfigGroup.h>
#include <PkSharedConfig.h>


#include <KoID.h>

#include "kis_debug.h"
#include "kis_serializable_configuration.h"


const char KisBookmarkedConfigurationManager::ConfigDefault[] = "Default";
const char KisBookmarkedConfigurationManager::ConfigLastUsed[] = "Last Used";

struct KisBookmarkedConfigurationManager::Private {

    PkString configEntryGroup;
    KisSerializableConfigurationFactory* configFactory;

};

KisBookmarkedConfigurationManager::KisBookmarkedConfigurationManager(const PkString & configEntryGroup, KisSerializableConfigurationFactory* configFactory)
    : d(new Private)
{
    d->configEntryGroup = configEntryGroup;
    d->configFactory = configFactory;
}

KisBookmarkedConfigurationManager::~KisBookmarkedConfigurationManager()
{
    delete d->configFactory;
    delete d;
}

KisSerializableConfigurationSP KisBookmarkedConfigurationManager::load(const PkString & configname) const
{
    if (!exists(configname)) {
        if (configname == KisBookmarkedConfigurationManager::ConfigDefault)
            return d->configFactory->createDefault();
        else
            return 0;
    }
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group(configEntryGroup());

    PkXmlDocument doc;
    doc.setContent(cfg.readEntry<PkString>(configname, ""));
    PkXmlElement e = doc.documentElement();
    KisSerializableConfigurationSP config = d->configFactory->create(e);
    dbgImage << config;
    return config;
}

void KisBookmarkedConfigurationManager::save(const PkString & configname, const KisSerializableConfigurationSP config)
{
    dbgImage << "Saving configuration " << config << " to " << configname;
    if (!config) return;
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group(configEntryGroup());
    cfg.writeEntry(configname, config->toXML());
}

bool KisBookmarkedConfigurationManager::exists(const PkString & configname) const
{
    PkConfigGroup cfg = PkSharedConfig::openConfig()->group(configEntryGroup());
    return cfg.hasKey(configname);
}

PkList<PkString> KisBookmarkedConfigurationManager::configurations() const
{
    // NOTE: 壳内 PkConfigStore 无键枚举接口（KConfig::entryMap 缺失），无法列出
    // 书签键名，返回空表。消费方在 plugins（壳闭包外）。
    return PkList<PkString>();
}

KisSerializableConfigurationSP KisBookmarkedConfigurationManager::defaultConfiguration() const
{
    if (exists(KisBookmarkedConfigurationManager::ConfigDefault)) {
        return load(KisBookmarkedConfigurationManager::ConfigDefault);
    }
    if (exists(KisBookmarkedConfigurationManager::ConfigLastUsed)) {
        return load(KisBookmarkedConfigurationManager::ConfigLastUsed);
    }
    return 0;
}

PkString KisBookmarkedConfigurationManager::configEntryGroup() const
{
    return d->configEntryGroup;
}

void KisBookmarkedConfigurationManager::remove(const PkString & name)
{
    PkSharedConfig::Ptr cfg = PkSharedConfig::openConfig();
    PkConfigGroup group = cfg->group(configEntryGroup());
    group.deleteEntry(name);
}

PkString KisBookmarkedConfigurationManager::uniqueName(const PkString & base)
{
#ifndef QT_NO_DEBUG
    PkString prev;
#endif
    int nb = 1;
    while (true) {
        PkString cur = base.arg(nb++);
        if (!exists(cur)) return cur;
#ifndef QT_NO_DEBUG
        Q_ASSERT(prev != cur);
        prev = cur;
#endif
    }
}
