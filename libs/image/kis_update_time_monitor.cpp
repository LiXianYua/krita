/*
 *  SPDX-FileCopyrightText: 2011 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_update_time_monitor.h"

struct KisUpdateTimeMonitor::Private
{
};

KisUpdateTimeMonitor::KisUpdateTimeMonitor()
    : m_d(new Private)
{
}

KisUpdateTimeMonitor::~KisUpdateTimeMonitor()
{
    delete m_d;
}

KisUpdateTimeMonitor* KisUpdateTimeMonitor::instance()
{
    static KisUpdateTimeMonitor instance;
    return &instance;
}

void KisUpdateTimeMonitor::startStrokeMeasure()
{
}

void KisUpdateTimeMonitor::endStrokeMeasure()
{
}

void KisUpdateTimeMonitor::reportPaintOpPreset(KisPaintOpPresetSP)
{
}

void KisUpdateTimeMonitor::reportMouseMove(const PkPointF &)
{
}

void KisUpdateTimeMonitor::printValues()
{
}

void KisUpdateTimeMonitor::reportJobStarted(void *)
{
}

void KisUpdateTimeMonitor::reportJobFinished(void *, const PkVector<PkRect> &)
{
}

void KisUpdateTimeMonitor::reportUpdateFinished(const PkRect &)
{
}
