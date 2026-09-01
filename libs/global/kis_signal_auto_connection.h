/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SIGNAL_AUTO_CONNECTOR_H
#define __KIS_SIGNAL_AUTO_CONNECTOR_H

#include <PkObject.h>
#include <PkConnect.h>
#include <PkSharedPointer.h>
#include <PkVector.h>

/**
 * A special wrapper class that represents a connection between two PkObject objects.
 * It creates the connection on the construction and disconnects it on destruction.
 *
 * WARNING: never use PkScopedPointer::reset() for updating the
 *          connection like:
 *
 * PkScopedPointer<KisSignalAutoConnection> conn;
 * ...
 * void Something::setCanvas(KoCanvasBase * canvas) {
 *     conn.reset(new KisSignalAutoConnection(...));
 * }
 *
 * The object stored in a scoped pointer will be destructed *after*
 * the new object created which will cause you object to become
 * disconnected.
 *
 * Instead use two-stage updates:
 * conn.reset();
 * conn.reset(new KisSignalAutoConnection(...));
 */
class KisSignalAutoConnection
{
public:
    /**
     * Creates a connection object and starts the requested connection
     */
    template<class Sender, class Signal, class Receiver, class Method>
    inline KisSignalAutoConnection(Sender sender, Signal signal,
                                  Receiver receiver, Method method,
                                  PkConnectionType type = PkConnectionType::Auto)
        : m_connection(PkObject::connect(sender, signal, receiver, method, type))
    {
    }

    inline ~KisSignalAutoConnection()
    {
        PkObject::disconnect(m_connection);
    }

private:
    KisSignalAutoConnection(const KisSignalAutoConnection &rhs);

private:
    PkMetaObject::Connection m_connection;
};

typedef PkSharedPointer<KisSignalAutoConnection> KisSignalAutoConnectionSP;

/**
 * A class to store multiple connections and to be able to stop all of
 * them at once. It is handy when you need to reconnect some other
 * object to the current manager. Then you just call
 * connectionsStore.clear() and then call addConnection() again to
 * recreate them.
 */
class KisSignalAutoConnectionsStore
{
public:
    /**
     * Connects \p sender to \p receiver with a connection of type \p type.
     * The connection is saved into the store so can be reset later with clear()
     *
     * \see addUniqueConnection()
     */
    template<class Sender, class Signal, class Receiver, class Method>
    inline void addConnection(Sender sender, Signal signal,
                              Receiver receiver, Method method,
                              PkConnectionType type = PkConnectionType::Auto)
    {
        m_connections.append(KisSignalAutoConnectionSP(
                                 new KisSignalAutoConnection(sender, signal,
                                                             receiver, method, type)));
    }

    /**
     * Convenience override for addConnection() that creates a unique connection
     *
     * \see addConnection()
     */
    template<class Sender, class Signal, class Receiver, class Method>
    inline void addUniqueConnection(Sender sender, Signal signal,
                                    Receiver receiver, Method method)
    {
        m_connections.append(KisSignalAutoConnectionSP(
                                 new KisSignalAutoConnection(sender, signal,
                                                             receiver, method, PkConnectionType::Unique)));
    }

    /**
     * Disconnects all the stored connections and removes them from the store
     */
    inline void clear() {
        m_connections.clear();
    }

    inline bool isEmpty() {
        return m_connections.isEmpty();
    }

private:
    PkVector<KisSignalAutoConnectionSP> m_connections;
};

#endif /* __KIS_SIGNAL_AUTO_CONNECTOR_H */
