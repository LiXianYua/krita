/*
 *  SPDX-FileCopyrightText: 2026
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_QT_CONNECTIONS_STORE_H
#define KIS_QT_CONNECTIONS_STORE_H

#include <QObject>
#include <QList>

/**
 * Qt-native counterpart of libs/global's KisSignalAutoConnectionsStore,
 * for files that keep real Qt (painting/kritashapemodel layer).
 *
 * The libs/global store was Pk-ized (PkObject::connect with a
 * `is_base_of<PkObject>` requirement), which cannot connect two real
 * QObjects. This store keeps the same interface (addConnection /
 * addUniqueConnection / clear / isEmpty) but uses QObject::connect, and
 * disconnects every stored connection on clear()/destruction.
 *
 * Header-only, so no build-system change is required to use it.
 */
class KisQtConnectionsStore
{
public:
    ~KisQtConnectionsStore()
    {
        clear();
    }

    template<class Sender, class Signal, class Receiver, class Method>
    void addConnection(Sender sender, Signal signal,
                       Receiver receiver, Method method,
                       Qt::ConnectionType type = Qt::AutoConnection)
    {
        m_connections.append(QObject::connect(sender, signal, receiver, method, type));
    }

    template<class Sender, class Signal, class Receiver, class Method>
    void addUniqueConnection(Sender sender, Signal signal,
                             Receiver receiver, Method method)
    {
        m_connections.append(QObject::connect(sender, signal, receiver, method, Qt::UniqueConnection));
    }

    void clear()
    {
        for (const QMetaObject::Connection &connection : m_connections) {
            QObject::disconnect(connection);
        }
        m_connections.clear();
    }

    bool isEmpty() const
    {
        return m_connections.isEmpty();
    }

private:
    QList<QMetaObject::Connection> m_connections;
};

#endif /* KIS_QT_CONNECTIONS_STORE_H */
