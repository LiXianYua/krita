/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ACYCLIC_SIGNAL_CONNECTOR_H
#define __KIS_ACYCLIC_SIGNAL_CONNECTOR_H

#include <compat/QObject>
#include "kritaglobal_export.h"
#include <mutex>

class KisAcyclicSignalConnector;

#include <PkVector.h>
#include <PkPointer.h>
#include <PkVariant.h>

/**
 * A special class for connecting UI elements to manager classes.
 * It allows to avoid direct calling blockSignals() for the sender UI
 * element all the time. This is the most important when the measured
 * value can be changed not only by the user through the UI, but also
 * by the manager according to some internal rules.
 *
 * Example:
 *
 * Suppose we have the following connections:
 *
 * 1) PkDoubleSpinBox::valueChanged(double) -> Manager::slotSetValue(double)
 * 2) Manager::valueChanged(double) -> PkDoubleSpinBox::setValue(double)
 *
 * Now if the manager decides to change/correct the value, the spinbox
 * will go into an infinite loop.
 *
 * See an example in KisToolCropConfigWidget.
 *
 * NOTE (coordinated connectors):
 *
 * Please make sure that you don't convert more than one forward and one backward
 * connection to the connector! If you do so, they will become connected to the
 * same forwarding slot and, therefore, both output signals will be emitted on
 * every incoming signal.
 *
 * To connect multiple connections that block recursive calls, please use
 * "coordinated connectors". Each such connector will have two more connection
 * slots that you can reuse.
 *
 */

class KRITAGLOBAL_EXPORT KisAcyclicSignalConnector : public PkObject
{
    Q_OBJECT
public:
    typedef std::unique_lock<KisAcyclicSignalConnector> Blocker;

public:

    KisAcyclicSignalConnector(PkObject *parent = 0);
    ~KisAcyclicSignalConnector();

    template <typename Sender, typename Signal, typename Receiver, typename Method>
    void connectForwardDouble(Sender *sender, Signal signal, Receiver *receiver, Method method)
    {
        connectTyped(sender, signal, receiver, method,
                     &KisAcyclicSignalConnector::forwardSlotDouble,
                     &KisAcyclicSignalConnector::forwardSignalDouble);
    }

    template <typename Sender, typename Signal, typename Receiver, typename Method>
    void connectBackwardDouble(Sender *sender, Signal signal, Receiver *receiver, Method method)
    {
        connectTyped(sender, signal, receiver, method,
                     &KisAcyclicSignalConnector::backwardSlotDouble,
                     &KisAcyclicSignalConnector::backwardSignalDouble);
    }

    template <typename Sender, typename Signal, typename Receiver, typename Method>
    void connectForwardInt(Sender *sender, Signal signal, Receiver *receiver, Method method)
    {
        connectTyped(sender, signal, receiver, method,
                     &KisAcyclicSignalConnector::forwardSlotInt,
                     &KisAcyclicSignalConnector::forwardSignalInt);
    }

    template <typename Sender, typename Signal, typename Receiver, typename Method>
    void connectBackwardInt(Sender *sender, Signal signal, Receiver *receiver, Method method)
    {
        connectTyped(sender, signal, receiver, method,
                     &KisAcyclicSignalConnector::backwardSlotInt,
                     &KisAcyclicSignalConnector::backwardSignalInt);
    }

    template <typename Sender, typename Signal, typename Receiver, typename Method>
    void connectForwardBool(Sender *sender, Signal signal, Receiver *receiver, Method method)
    {
        connectTyped(sender, signal, receiver, method,
                     &KisAcyclicSignalConnector::forwardSlotBool,
                     &KisAcyclicSignalConnector::forwardSignalBool);
    }

    template <typename Sender, typename Signal, typename Receiver, typename Method>
    void connectBackwardBool(Sender *sender, Signal signal, Receiver *receiver, Method method)
    {
        connectTyped(sender, signal, receiver, method,
                     &KisAcyclicSignalConnector::backwardSlotBool,
                     &KisAcyclicSignalConnector::backwardSignalBool);
    }

    template <typename Sender, typename Signal, typename Receiver, typename Method>
    void connectForwardVoid(Sender *sender, Signal signal, Receiver *receiver, Method method)
    {
        connectTyped(sender, signal, receiver, method,
                     &KisAcyclicSignalConnector::forwardSlotVoid,
                     &KisAcyclicSignalConnector::forwardSignalVoid);
    }

    template <typename Sender, typename Signal, typename Receiver, typename Method>
    void connectBackwardVoid(Sender *sender, Signal signal, Receiver *receiver, Method method)
    {
        connectTyped(sender, signal, receiver, method,
                     &KisAcyclicSignalConnector::backwardSlotVoid,
                     &KisAcyclicSignalConnector::backwardSignalVoid);
    }

    template <typename Sender, typename Signal, typename Receiver, typename Method>
    void connectForwardVariant(Sender *sender, Signal signal, Receiver *receiver, Method method)
    {
        connectTyped(sender, signal, receiver, method,
                     &KisAcyclicSignalConnector::forwardSlotVariant,
                     &KisAcyclicSignalConnector::forwardSignalVariant);
    }

    template <typename Sender, typename Signal, typename Receiver, typename Method>
    void connectBackwardVariant(Sender *sender, Signal signal, Receiver *receiver, Method method)
    {
        connectTyped(sender, signal, receiver, method,
                     &KisAcyclicSignalConnector::backwardSlotVariant,
                     &KisAcyclicSignalConnector::backwardSignalVariant);
    }

    template <typename Sender, typename Signal, typename Receiver, typename Method>
    void connectForwardResourcePair(Sender *sender, Signal signal, Receiver *receiver, Method method)
    {
        connectTyped(sender, signal, receiver, method,
                     &KisAcyclicSignalConnector::forwardSlotResourcePair,
                     &KisAcyclicSignalConnector::forwardSignalResourcePair);
    }

    template <typename Sender, typename Signal, typename Receiver, typename Method>
    void connectBackwardResourcePair(Sender *sender, Signal signal, Receiver *receiver, Method method)
    {
        connectTyped(sender, signal, receiver, method,
                     &KisAcyclicSignalConnector::backwardSlotResourcePair,
                     &KisAcyclicSignalConnector::backwardSignalResourcePair);
    }

    /**
     * Lock the connector and all its coordinated child connectors
     */
    void lock();

    /**
     * Unlock the connector and all its coordinated child connectors
     */
    void unlock();

    /**
     * \return true if the connector is locked by some signal or manually.
     * Used for debugging purposes mostly.
     */
    bool isLocked() const;

    /**
     * @brief create a coordinated connector that can be used for extending
     *        the number of self-locking connection.
     *
     * The coordinated connector can be used to extend the number of self-locking
     * connections. Each coordinated connector adds two more connection slots (forward
     * and backward).  Lock of any connector in a coordinated group will lock the whole
     * group.
     *
     * The created connector is owned by *this, don't delete it!
     */
    KisAcyclicSignalConnector *createCoordinatedConnector();

private:

    template <typename Sender, typename Signal, typename Receiver, typename Method,
              typename RelaySlot, typename RelaySignal>
    void connectTyped(Sender *sender, Signal signal, Receiver *receiver, Method method,
                      RelaySlot relaySlot, RelaySignal relaySignal)
    {
        PkObject::connect(sender, signal, this, relaySlot, PkConnectionType::Unique);
        PkObject::connect(this, relaySignal, receiver, method, PkConnectionType::Unique);
    }

    /**
     * Lock this connector only.
     */
    void coordinatedLock();

    /**
     * Unlock this connector only.
     */
    void coordinatedUnlock();

private Q_SLOTS:
    void forwardSlotDouble(double value);
    void backwardSlotDouble(double value);

    void forwardSlotInt(int value);
    void backwardSlotInt(int value);

    void forwardSlotBool(bool value);
    void backwardSlotBool(bool value);

    void forwardSlotVoid();
    void backwardSlotVoid();

    void forwardSlotVariant(const PkVariant &value);
    void backwardSlotVariant(const PkVariant &value);

    void forwardSlotResourcePair(int key, const PkVariant &resource);
    void backwardSlotResourcePair(int key, const PkVariant &resource);

Q_SIGNALS:
    void forwardSignalDouble(double value);
    void backwardSignalDouble(double value);

    void forwardSignalInt(int value);
    void backwardSignalInt(int value);

    void forwardSignalBool(bool value);
    void backwardSignalBool(bool value);

    void forwardSignalVoid();
    void backwardSignalVoid();

    void forwardSignalVariant(const PkVariant &value);
    void backwardSignalVariant(const PkVariant &value);

    void forwardSignalResourcePair(int key, const PkVariant &value);
    void backwardSignalResourcePair(int key, const PkVariant &value);

private:
    int m_signalsBlocked;
    PkVector<PkPointer<KisAcyclicSignalConnector>> m_coordinatedConnectors;
    PkPointer<KisAcyclicSignalConnector> m_parentConnector;
};

#endif /* __KIS_ACYCLIC_SIGNAL_CONNECTOR_H */
