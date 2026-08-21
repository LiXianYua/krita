/*
 *  SPDX-FileCopyrightText: 2007 Adrian Page <adrian@pagenet.plus.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "kis_config_notifier.h"

#include <kis_debug.h>
#include "kis_signal_compressor.h"

static KisConfigNotifier *s_instance()
{
    static KisConfigNotifier instance;
    return &instance;
}

struct KisConfigNotifier::Private
{
    Private() : dropFramesModeCompressor(300, KisSignalCompressor::FIRST_ACTIVE) {}

    KisSignalCompressor dropFramesModeCompressor;
};

KisConfigNotifier::KisConfigNotifier()
    : m_d(new Private)
{
    PkObject::connect(&m_d->dropFramesModeCompressor, &KisSignalCompressor::timeout,
                      this, &KisConfigNotifier::dropFramesModeChanged);
}

KisConfigNotifier::~KisConfigNotifier()
{
    dbgRegistry << "deleting KisConfigNotifier";
}

KisConfigNotifier *KisConfigNotifier::instance()
{
    return s_instance();
}

void KisConfigNotifier::notifyConfigChanged(void)
{
    Q_EMIT configChanged();
}

void KisConfigNotifier::notifyDropFramesModeChanged()
{
    m_d->dropFramesModeCompressor.start();
}

void KisConfigNotifier::notifyPixelGridModeChanged()
{
    Q_EMIT pixelGridModeChanged();
}

void KisConfigNotifier::notifyColorHistoryModeChanged()
{
    Q_EMIT colorHistoryModeChanged();
}

void KisConfigNotifier::notifyTouchPaintingChanged()
{
    Q_EMIT touchPaintingChanged();
}

void KisConfigNotifier::notifyColorSamplerPreviewStyleChanged()
{
    Q_EMIT sigColorSamplerPreviewStyleChanged();
}

void KisConfigNotifier::notifyColorThemeChanged(const PkString &filename)
{
    Q_EMIT signalColorThemeChanged(filename);
}

void KisConfigNotifier::notifyLongPressChanged(bool enabled)
{
    Q_EMIT sigLongPressChanged(enabled);
}

#ifdef Q_OS_ANDROID
void KisConfigNotifier::notifyUsePageUpDownMouseButtonEmulationWorkaroundChanged(bool enabled)
{
    Q_EMIT sigUsePageUpDownMouseButtonEmulationWorkaroundChanged(enabled);
}

void KisConfigNotifier::notifyUseIgnoreHistoricTabletEventsWorkaroundChanged(bool enabled)
{
    Q_EMIT sigUseIgnoreHistoricTabletEventsWorkaroundChanged(enabled);
}
#endif
