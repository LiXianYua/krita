/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KISRECTSCOLLISIONSTRACKER_H
#define KISRECTSCOLLISIONSTRACKER_H

#include <mutex>

#include "PkList.h"
#include "PkRect.h"

#include "kis_assert.h"


class KisRectsCollisionsTracker
{
public:

    void startAccessingRect(const PkRect &rc) {
        std::lock_guard<std::mutex> l(m_mutex);

        checkUniqueAccessImpl(rc, "start");
        m_rectsInProgress.append(rc);
    }

    void endAccessingRect(const PkRect &rc) {
        std::lock_guard<std::mutex> l(m_mutex);
        const bool result = m_rectsInProgress.removeOne(rc);
        KIS_SAFE_ASSERT_RECOVER_NOOP(result);
        checkUniqueAccessImpl(rc, "end");
    }

private:

    bool checkUniqueAccessImpl(const PkRect &rect, const char *tag) {

        for (const PkRect &rc : m_rectsInProgress) {
            if (rc != rect && rect.intersects(rc)) {
                ENTER_FUNCTION() << "FAIL: concurrent access from"
                                 << rect.x() << rect.y() << rect.width() << rect.height()
                                 << "to"
                                 << rc.x() << rc.y() << rc.width() << rc.height()
                                 << tag;
                return false;
            }
        }

        return true;
    }

private:
    PkList<PkRect> m_rectsInProgress;
    std::mutex m_mutex;
};

#endif // KISRECTSCOLLISIONSTRACKER_H
