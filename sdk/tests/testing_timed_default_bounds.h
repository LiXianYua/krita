/*
 *  SPDX-FileCopyrightText: 2015 Jouni Pentikäinen <joupent@gmail.com>
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __TESTING_TIMED_DEFAULT_BOUNDS_H
#define __TESTING_TIMED_DEFAULT_BOUNDS_H

#include "kis_default_bounds_base.h"
#include <PkRect.h>


namespace TestUtil {

struct TestingTimedDefaultBounds : public KisDefaultBoundsBase {
    TestingTimedDefaultBounds(const PkRect &bounds = PkRect(0,0,100,100))
        : m_time(0),
          m_lod(0),
          m_bounds(bounds)
    {
    }

    PkRect bounds() const override {
        return m_bounds;
    }

    bool wrapAroundMode() const override {
        return false;
    }

    WrapAroundAxis wrapAroundModeAxis() const override {
        return WRAPAROUND_BOTH;
    }

    int currentLevelOfDetail() const override {
        return m_lod;
    }

    int currentTime() const override {
        return m_time;
    }

    bool externalFrameActive() const override {
        return false;
    }

    void testingSetTime(int time) {
        m_time = time;
    }

    void testingSetLod(int lod) {
        m_lod = lod;
    }

    void * sourceCookie() const override {
        return 0;
    }

private:
    int m_time;
    int m_lod;
    PkRect m_bounds;
};

}

#endif /* __TESTING_TIMED_DEFAULT_BOUNDS_H */
