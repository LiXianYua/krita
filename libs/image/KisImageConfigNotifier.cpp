/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisImageConfigNotifier.h"


#include <kis_debug.h>
#include "kis_signal_compressor.h"

static KisImageConfigNotifier *s_instance()
{
    static KisImageConfigNotifier instance;
    return &instance;
}

struct KisImageConfigNotifier::Private
{
    Private()
        : updateCompressor(300, KisSignalCompressor::FIRST_ACTIVE)
        , autoKeyframeUpdateCompressor(300, KisSignalCompressor::FIRST_ACTIVE)
    {}

    KisSignalCompressor updateCompressor;
    KisSignalCompressor autoKeyframeUpdateCompressor;
};

KisImageConfigNotifier::KisImageConfigNotifier()
    : m_d(new Private)
{
    PkObject::connect(&m_d->updateCompressor, &KisSignalCompressor::timeout,
                      this, &KisImageConfigNotifier::configChanged);
    PkObject::connect(&m_d->updateCompressor, &KisSignalCompressor::timeout,
                      this, &KisImageConfigNotifier::autoKeyFrameConfigurationChanged);
    PkObject::connect(&m_d->autoKeyframeUpdateCompressor, &KisSignalCompressor::timeout,
                      this, &KisImageConfigNotifier::autoKeyFrameConfigurationChanged);
}

KisImageConfigNotifier::~KisImageConfigNotifier()
{
}

KisImageConfigNotifier *KisImageConfigNotifier::instance()
{
    return s_instance();
}

void KisImageConfigNotifier::notifyConfigChanged()
{
    m_d->updateCompressor.start();
}

void KisImageConfigNotifier::notifyAutoKeyFrameConfigurationChanged()
{
    m_d->autoKeyframeUpdateCompressor.start();
}

void KisImageConfigNotifier::notifyGlobalProofingConfigChanged()
{
    globalProofingConfigChanged();
}
