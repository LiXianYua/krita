/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_acyclic_signal_connector.h"

#include "kis_debug.h"

KisAcyclicSignalConnector::KisAcyclicSignalConnector(PkObject *parent)
    : PkObject(parent),
      m_signalsBlocked(0)
{
}

KisAcyclicSignalConnector::~KisAcyclicSignalConnector()
{
}

void KisAcyclicSignalConnector::lock()
{
    if (m_parentConnector) {
        m_parentConnector->lock();
    } else {
        coordinatedLock();

        for (PkPointer<KisAcyclicSignalConnector> conn : m_coordinatedConnectors) {
            if (!conn) continue;
            conn->coordinatedLock();
        }
    }
}

void KisAcyclicSignalConnector::unlock()
{
    if (m_parentConnector) {
        m_parentConnector->unlock();
    } else {
        for (PkPointer<KisAcyclicSignalConnector> conn : m_coordinatedConnectors) {
            if (!conn) continue;
            conn->coordinatedUnlock();
        }

        coordinatedUnlock();
    }
}

bool KisAcyclicSignalConnector::isLocked() const
{
    return m_signalsBlocked;
}

void KisAcyclicSignalConnector::coordinatedLock()
{
    m_signalsBlocked++;
}

void KisAcyclicSignalConnector::coordinatedUnlock()
{
    m_signalsBlocked--;
}

KisAcyclicSignalConnector *KisAcyclicSignalConnector::createCoordinatedConnector()
{
    KisAcyclicSignalConnector *conn = new KisAcyclicSignalConnector(this);
    conn->m_parentConnector = this;
    m_coordinatedConnectors.append(conn);
    return conn;
}
