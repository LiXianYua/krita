/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *  SPDX-FileCopyrightText: 2014 Mohit Goyal <mohit.bits2011@gmail.com>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */
#include <brushengine/kis_locked_properties_proxy.h>

#include <KoResource.h>
#include <kis_debug.h>
#include <PkString.h>
#include <PkVariant.h>
#include <PkList.h>
#include <PkSet.h>
#include <KisDirtyStateSaver.h>

#include <brushengine/kis_locked_properties.h>
#include <brushengine/kis_locked_properties_server.h>
#include <brushengine/kis_paintop_settings.h>
#include <brushengine/kis_paintop_preset.h>


KisLockedPropertiesProxy::KisLockedPropertiesProxy(KisPropertiesConfiguration *p, KisLockedPropertiesSP l)
{
    m_parent = p;
    m_lockedProperties = l;
}

KisLockedPropertiesProxy::~KisLockedPropertiesProxy()
{
}

PkVariant KisLockedPropertiesProxy::getProperty(const PkString &name) const
{
    KisPaintOpSettings *t = dynamic_cast<KisPaintOpSettings*>(m_parent);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(t, m_parent->getProperty(name));
    if (!t->updateListener()) return m_parent->getProperty(name);

    // restores the dirty state on returns automagically
    KisPaintOpSettings::UpdateListenerSP updateProxy = t->updateListener().toStrongRef();
    KisDirtyStateSaver<KisPaintOpSettings::UpdateListenerSP> dirtyStateSaver(updateProxy);

    if (m_lockedProperties->lockedProperties()) {
        if (m_lockedProperties->lockedProperties()->hasProperty(name)) {
            KisLockedPropertiesServer::instance()->setPropertiesFromLocked(true);

            if (!m_parent->hasProperty(name + "_previous")) {
                m_parent->setProperty(name + "_previous", m_parent->getProperty(name));
                m_parent->setPropertyNotSaved(name + "_previous");
            }

            const PkVariant lockedProp = m_lockedProperties->lockedProperties()->getProperty(name);

            if (m_parent->getProperty(name) != lockedProp) {
                m_parent->setProperty(name, lockedProp);
            }

            return lockedProp;
        } else {
            if (m_parent->hasProperty(name + "_previous")) {
                m_parent->setProperty(name, m_parent->getProperty(name + "_previous"));
                m_parent->removeProperty(name + "_previous");
            }
        }
    }

    return m_parent->getProperty(name);
}

void KisLockedPropertiesProxy::setProperty(const PkString & name, const PkVariant & value)
{
    KisPaintOpSettings *t = dynamic_cast<KisPaintOpSettings*>(m_parent);
    KIS_SAFE_ASSERT_RECOVER_RETURN(t);
    if (!t->updateListener()) return;

    if (m_lockedProperties->lockedProperties()) {
        if (m_lockedProperties->lockedProperties()->hasProperty(name)) {
            m_lockedProperties->lockedProperties()->setProperty(name, value);
            m_parent->setProperty(name, value);

            if (!m_parent->hasProperty(name + "_previous")) {
                // restores the dirty state on returns automagically
                KisPaintOpSettings::UpdateListenerSP updateProxy = t->updateListener().toStrongRef();
                KisDirtyStateSaver<KisPaintOpSettings::UpdateListenerSP> dirtyStateSaver(updateProxy);
                m_parent->setProperty(name + "_previous", m_parent->getProperty(name));
                m_parent->setPropertyNotSaved(name + "_previous");
            }
            return;
        }
    }

    m_parent->setProperty(name, value);
}

bool KisLockedPropertiesProxy::hasProperty(const PkString &name) const
{
    KisPaintOpSettings *t = dynamic_cast<KisPaintOpSettings*>(m_parent);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(t, m_parent->hasProperty(name));
    if (!t->updateListener()) return m_parent->hasProperty(name);

    return (m_lockedProperties->lockedProperties() &&
            m_lockedProperties->lockedProperties()->hasProperty(name)) ||
            m_parent->hasProperty(name);

}

PkList<PkString> KisLockedPropertiesProxy::getPropertiesKeys() const
{
    KisPaintOpSettings *t = dynamic_cast<KisPaintOpSettings*>(m_parent);
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(t, m_parent->getPropertiesKeys());
    if (!t->updateListener()) return m_parent->getPropertiesKeys();

    PkList<PkString> result = m_parent->getPropertiesKeys();

    if (m_lockedProperties->lockedProperties() && !m_lockedProperties->lockedProperties()->getPropertiesKeys().isEmpty()) {
        PkSet<PkString> properties;
        for (const PkString &key : result) {
            properties.insert(key);
        }
        auto lockedPropertiesKeys = m_lockedProperties->lockedProperties()->getPropertiesKeys();
        for (const PkString &key : lockedPropertiesKeys) {
            properties.insert(key);
        }
        result.clear();
        for (const PkString &key : properties) {
            result.append(key);
        }
    }

    return result;
}

void KisLockedPropertiesProxy::dump() const
{
    dbgRegistry << "=== KisLockedPropertiesProxy::dump() ===";
    dbgRegistry << "parent properties:";
    m_parent->dump();

    if (m_lockedProperties->lockedProperties()) {
        dbgRegistry << "locked properties:";
        m_lockedProperties->lockedProperties()->dump();
    }
}



