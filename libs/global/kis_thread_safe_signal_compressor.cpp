/*
 *  SPDX-FileCopyrightText: 2014 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_thread_safe_signal_compressor.h"

#include <PkThread.h>

KisThreadSafeSignalCompressor::KisThreadSafeSignalCompressor(int delay, KisSignalCompressor::Mode mode)
    : m_compressor(new KisSignalCompressor(delay, mode, this))
{
    PkObject::connect(this, &KisThreadSafeSignalCompressor::internalRequestSignal,
                      m_compressor, &KisSignalCompressor::start, PkConnectionType::Auto);
    PkObject::connect(this, &KisThreadSafeSignalCompressor::internalStopSignal,
                      m_compressor, &KisSignalCompressor::stop, PkConnectionType::Auto);
    PkObject::connect(
        this, &KisThreadSafeSignalCompressor::internalSetDelay,
        m_compressor,
        static_cast<void (KisSignalCompressor::*)(int)>(&KisSignalCompressor::setDelay),
        PkConnectionType::Auto);
    PkObject::connect(m_compressor, &KisSignalCompressor::timeout,
                      this, &KisThreadSafeSignalCompressor::timeout,
                      PkConnectionType::Auto);

    const PkThreadId mainThread = PkThread::mainThreadId();
    this->moveToThread(mainThread);
    m_compressor->moveToThread(mainThread);
}

bool KisThreadSafeSignalCompressor::isActive() const
{
    return m_compressor->isActive();
}

void KisThreadSafeSignalCompressor::setDelay(int delay)
{
    internalSetDelay(delay);
}

void KisThreadSafeSignalCompressor::start()
{
    internalRequestSignal();
}

void KisThreadSafeSignalCompressor::stop()
{
    internalStopSignal();
}
