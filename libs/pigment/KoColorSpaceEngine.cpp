/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <PkXmlCompat.h>

#include <KoColorSpaceEngine.h>

struct KoColorSpaceEngine::Private {
    PkString id;
    PkString name;
};

KoColorSpaceEngine::KoColorSpaceEngine(const PkString& id, const PkString& name) : d(new Private)
{
    d->id = id;
    d->name = name;
}

KoColorSpaceEngine::~KoColorSpaceEngine()
{
    delete d;
}

const PkString& KoColorSpaceEngine::id() const
{
    return d->id;
}

const PkString& KoColorSpaceEngine::name() const
{
    return d->name;
}

bool KoColorSpaceEngine::supportsColorSpace(const PkString &colorModelId, const PkString &colorDepthId, const KoColorProfile *profile) const
{
    Q_UNUSED(colorModelId);
    Q_UNUSED(colorDepthId);
    Q_UNUSED(profile);

    return true;
}

KoColorSpaceEngineRegistry::KoColorSpaceEngineRegistry()
{
}

KoColorSpaceEngineRegistry::~KoColorSpaceEngineRegistry()
{
    for (KoColorSpaceEngine* item : values()) {
        delete item;
    }
}

KoColorSpaceEngineRegistry* KoColorSpaceEngineRegistry::instance()
{
    static KoColorSpaceEngineRegistry s_instance;
    return &s_instance;
}
