/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2014 Mohit Goyal <mohit.bits2011@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <brushengine/kis_locked_properties_server.h>
#include <brushengine/kis_locked_properties.h>





KisLockedPropertiesServer::KisLockedPropertiesServer()
{
    m_lockedProperties = new KisLockedProperties();
    m_propertiesFromLocked = false;
}

KisLockedPropertiesServer::~KisLockedPropertiesServer()
{
}

KisLockedPropertiesProxySP KisLockedPropertiesServer::createLockedPropertiesProxy(KisPropertiesConfiguration *settings)
{
    return new KisLockedPropertiesProxy(settings, lockedProperties());
}

KisLockedPropertiesProxySP KisLockedPropertiesServer::createLockedPropertiesProxy(KisPropertiesConfigurationSP settings)
{
    return createLockedPropertiesProxy(settings.data());
}

KisLockedPropertiesServer* KisLockedPropertiesServer::instance()
{
    static KisLockedPropertiesServer s_instance;
    return &s_instance;
}

KisLockedPropertiesSP KisLockedPropertiesServer::lockedProperties()
{
    return m_lockedProperties;
}

void KisLockedPropertiesServer::addToLockedProperties(KisPropertiesConfigurationSP p)
{
    lockedProperties()->addToLockedProperties(p);
}

void KisLockedPropertiesServer::removeFromLockedProperties(KisPropertiesConfigurationSP p)
{
    lockedProperties()->removeFromLockedProperties(p);
}

void KisLockedPropertiesServer::setPropertiesFromLocked(bool value)
{
    m_propertiesFromLocked = value;
}

bool KisLockedPropertiesServer::propertiesFromLocked()
{
    return m_propertiesFromLocked;
}
bool KisLockedPropertiesServer::hasProperty(const PkString &p)
{
    return m_lockedProperties->hasProperty(p);
}



