/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_SIGNALS_BLOCKER_H
#define __KIS_SIGNALS_BLOCKER_H

#include <PkObject.h>
#include <PkConnect.h>
#include <PkVector.h>

/**
 * Block PkObject's signals in a safe and sane way.
 *
 * Avoid using direct calls to PkObject::blockSignals(bool),
 * because:
 *
 * 1) They are not safe. One beautifully sunny day someone (it might
 *    easily be you yourself) will forget about these call and will put
 *    a 'return' statement somewhere among the lines. Surely this is
 *    not what you expect to happen.
 *
 * 2) Two lines of blocking for every line of access can easily make
 *    the code unreadable.
 */

class KisSignalsBlocker
{
public:
    /**
     * Six should be enough for all usage cases! (c)
     */
    KisSignalsBlocker(PkObject *o1,
                      PkObject *o2,
                      PkObject *o3 = 0,
                      PkObject *o4 = 0,
                      PkObject *o5 = 0,
                      PkObject *o6 = 0)
    {
        if (o1) addObject(o1);
        if (o2) addObject(o2);
        if (o3) addObject(o3);
        if (o4) addObject(o4);
        if (o5) addObject(o5);
        if (o6) addObject(o6);

        blockObjects();
    }

    KisSignalsBlocker(PkObject *object)
    {
        addObject(object);
        blockObjects();
    }

    ~KisSignalsBlocker()
    {
        auto it = m_objects.end();
        auto begin = m_objects.begin();

        while (it != begin) {
            --it;
            it->first->blockSignals(it->second);
        }
    }

private:
    void blockObjects() {
        for (auto it = m_objects.begin(); it != m_objects.end(); ++it) {
            it->first->blockSignals(true);
        }
    }

    inline void addObject(PkObject *object) {
        m_objects.append(qMakePair(object, object->signalsBlocked()));
    }

private:
    Q_DISABLE_COPY(KisSignalsBlocker)

private:
    PkVector<PkPair<PkObject*,bool>> m_objects;
};

#endif /* __KIS_SIGNALS_BLOCKER_H */
