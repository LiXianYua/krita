/*
 *  SPDX-FileCopyrightText: 2015 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_color_manager.h"

static KisColorManager *s_instance()
{
    static KisColorManager *s = new KisColorManager();
    return s;
}

class KisColorManager::Private {
public:
    Private(PkObject *)
    {}
};

KisColorManager::KisColorManager()
    : PkObject()
    , d(new Private(this))
{
}

KisColorManager::~KisColorManager()
{
    delete d;
}

PkString KisColorManager::deviceName(const PkString &)
{
    return PkString();
}

PkStringList KisColorManager::devices(DeviceType ) const
{
    return PkStringList();
}

PkByteArray KisColorManager::displayProfile(const PkString &, int ) const
{
    return PkByteArray();
}

KisColorManager *KisColorManager::instance()
{
    return s_instance();
}
