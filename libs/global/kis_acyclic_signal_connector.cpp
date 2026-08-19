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

void KisAcyclicSignalConnector::connectForwardDouble(PkObject *sender, const char *signal,
                                                  PkObject *receiver, const char *method)
{

    connect(sender, signal, this, SLOT(forwardSlotDouble(double)), Pk::UniqueConnection);
    connect(this, SIGNAL(forwardSignalDouble(double)), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectBackwardDouble(PkObject *sender, const char *signal,
                                                   PkObject *receiver, const char *method)
{

    connect(sender, signal, this, SLOT(backwardSlotDouble(double)), Pk::UniqueConnection);
    connect(this, SIGNAL(backwardSignalDouble(double)), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectForwardInt(PkObject *sender, const char *signal,
                                                  PkObject *receiver, const char *method)
{

    connect(sender, signal, this, SLOT(forwardSlotInt(int)), Pk::UniqueConnection);
    connect(this, SIGNAL(forwardSignalInt(int)), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectBackwardInt(PkObject *sender, const char *signal,
                                                   PkObject *receiver, const char *method)
{

    connect(sender, signal, this, SLOT(backwardSlotInt(int)), Pk::UniqueConnection);
    connect(this, SIGNAL(backwardSignalInt(int)), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectForwardBool(PkObject *sender, const char *signal,
                                                  PkObject *receiver, const char *method)
{

    connect(sender, signal, this, SLOT(forwardSlotBool(bool)), Pk::UniqueConnection);
    connect(this, SIGNAL(forwardSignalBool(bool)), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectBackwardBool(PkObject *sender, const char *signal,
                                                   PkObject *receiver, const char *method)
{

    connect(sender, signal, this, SLOT(backwardSlotBool(bool)), Pk::UniqueConnection);
    connect(this, SIGNAL(backwardSignalBool(bool)), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectForwardVoid(PkObject *sender, const char *signal,
                                                  PkObject *receiver, const char *method)
{

    connect(sender, signal, this, SLOT(forwardSlotVoid()), Pk::UniqueConnection);
    connect(this, SIGNAL(forwardSignalVoid()), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectBackwardVoid(PkObject *sender, const char *signal,
                                                 PkObject *receiver, const char *method)
{

    connect(sender, signal, this, SLOT(backwardSlotVoid()), Pk::UniqueConnection);
    connect(this, SIGNAL(backwardSignalVoid()), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectForwardVariant(PkObject *sender, const char *signal,
                                                  PkObject *receiver, const char *method)
{

    connect(sender, signal, this, SLOT(forwardSlotVariant(PkVariant)), Pk::UniqueConnection);
    connect(this, SIGNAL(forwardSignalVariant(PkVariant)), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectBackwardVariant(PkObject *sender, const char *signal,
                                                       PkObject *receiver, const char *method)
{
    connect(sender, signal, this, SLOT(backwardSlotVariant(PkVariant)), Pk::UniqueConnection);
    connect(this, SIGNAL(backwardSignalVariant(PkVariant)), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectForwardResourcePair(PkObject *sender, const char *signal, PkObject *receiver, const char *method)
{
    connect(sender, signal, this, SLOT(forwardSlotResourcePair(int,PkVariant)), Pk::UniqueConnection);
    connect(this, SIGNAL(forwardSignalResourcePair(int,PkVariant)), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectBackwardResourcePair(PkObject *sender, const char *signal, PkObject *receiver, const char *method)
{
    connect(sender, signal, this, SLOT(backwardSlotResourcePair(int,PkVariant)), Pk::UniqueConnection);
    connect(this, SIGNAL(backwardSignalResourcePair(int,PkVariant)), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectForwardKoColor(PkObject *sender, const char *signal, PkObject *receiver, const char *method)
{
    connect(sender, signal, this, SLOT(forwardSlotKoColor(KoColor)), Pk::UniqueConnection);
    connect(this, SIGNAL(forwardSignalKoColor(KoColor)), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::connectBackwardKoColor(PkObject *sender, const char *signal, PkObject *receiver, const char *method)
{
    connect(sender, signal, this, SLOT(backwardSlotKoColor(KoColor)), Pk::UniqueConnection);
    connect(this, SIGNAL(backwardSignalKoColor(KoColor)), receiver, method, Pk::UniqueConnection);
}

void KisAcyclicSignalConnector::lock()
{
    if (m_parentConnector) {
        m_parentConnector->lock();
    } else {
        coordinatedLock();

        Q_FOREACH(PkPointer<KisAcyclicSignalConnector> conn, m_coordinatedConnectors) {
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
        Q_FOREACH(PkPointer<KisAcyclicSignalConnector> conn, m_coordinatedConnectors) {
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

void KisAcyclicSignalConnector::forwardSlotDouble(double value)
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT forwardSignalDouble(value);
    unlock();
}

void KisAcyclicSignalConnector::backwardSlotDouble(double value)
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT backwardSignalDouble(value);
    unlock();
}

void KisAcyclicSignalConnector::forwardSlotInt(int value)
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT forwardSignalInt(value);
    unlock();
}

void KisAcyclicSignalConnector::backwardSlotInt(int value)
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT backwardSignalInt(value);
    unlock();
}

void KisAcyclicSignalConnector::forwardSlotBool(bool value)
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT forwardSignalBool(value);
    unlock();
}

void KisAcyclicSignalConnector::backwardSlotBool(bool value)
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT backwardSignalBool(value);
    unlock();
}

void KisAcyclicSignalConnector::forwardSlotVoid()
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT forwardSignalVoid();
    unlock();
}

void KisAcyclicSignalConnector::backwardSlotVoid()
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT backwardSignalVoid();
    unlock();
}

void KisAcyclicSignalConnector::forwardSlotVariant(const PkVariant &value)
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT forwardSignalVariant(value);
    unlock();
}

void KisAcyclicSignalConnector::backwardSlotVariant(const PkVariant &value)
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT backwardSignalVariant(value);
    unlock();
}

void KisAcyclicSignalConnector::forwardSlotResourcePair(int key, const PkVariant &resource)
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT forwardSignalResourcePair(key, resource);
    unlock();
}

void KisAcyclicSignalConnector::backwardSlotResourcePair(int key, const PkVariant &resource)
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT backwardSignalResourcePair(key, resource);
    unlock();
}

void KisAcyclicSignalConnector::forwardSlotKoColor(const KoColor &value)
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT forwardSignalKoColor(value);
    unlock();
}

void KisAcyclicSignalConnector::backwardSlotKoColor(const KoColor &value)
{
    if (m_signalsBlocked) return;

    lock();
    Q_EMIT backwardSignalKoColor(value);
    unlock();
}
