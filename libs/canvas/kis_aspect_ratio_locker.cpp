/*
 * SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_aspect_ratio_locker.h"

struct KisAspectRatioLocker::Private
{
    KisAspectRatioValueControl *spinOne = nullptr;
    KisAspectRatioValueControl *spinTwo = nullptr;
    KisAspectRatioToggleControl *aspectButton = nullptr;
    qreal aspectRatio = 1.0;
    bool blockUpdatesOnDrag = false;
    bool correctingValue = false;
    bool suppressSignals = false;
};

KisAspectRatioLocker::KisAspectRatioLocker(PkObject *parent)
    : PkObject(parent)
    , m_d(new Private)
{
}

KisAspectRatioLocker::~KisAspectRatioLocker() = default;

void KisAspectRatioLocker::connectSpinBoxes(KisAspectRatioValueControl *spinOne,
                                            KisAspectRatioValueControl *spinTwo,
                                            KisAspectRatioToggleControl *aspectButton)
{
    m_d->spinOne = spinOne;
    m_d->spinTwo = spinTwo;
    m_d->aspectButton = aspectButton;

    spinOne->setValueChangedCallback([this]() { slotSpinOneChanged(); });
    spinTwo->setValueChangedCallback([this]() { slotSpinTwoChanged(); });
    aspectButton->setToggledCallback([this](bool) { slotAspectButtonChanged(); });
    slotAspectButtonChanged();
}

void KisAspectRatioLocker::slotSpinOneChanged()
{
    if (m_d->correctingValue) {
        return;
    }

    if (m_d->aspectButton->isChecked()) {
        m_d->correctingValue = true;
        m_d->spinTwo->setValue(m_d->aspectRatio * m_d->spinOne->value());
        m_d->correctingValue = false;
    }

    if (!m_d->suppressSignals) {
        sliderValueChanged();
    }
}

void KisAspectRatioLocker::slotSpinTwoChanged()
{
    if (m_d->correctingValue) {
        return;
    }

    if (m_d->aspectButton->isChecked()) {
        m_d->correctingValue = true;
        m_d->spinOne->setValue(m_d->spinTwo->value() / m_d->aspectRatio);
        m_d->correctingValue = false;
    }

    if (!m_d->suppressSignals) {
        sliderValueChanged();
    }
}

void KisAspectRatioLocker::slotAspectButtonChanged()
{
    const bool checked = m_d->aspectButton->isChecked();
    if (checked && m_d->spinTwo->value() > 0 && m_d->spinOne->value() > 0) {
        m_d->aspectRatio = m_d->spinTwo->value() / m_d->spinOne->value();
    } else {
        m_d->aspectRatio = 1.0;
    }

    if (!m_d->suppressSignals) {
        aspectButtonChanged();
        aspectButtonToggled(checked);
    }
}

void KisAspectRatioLocker::slotSpinDraggingFinished()
{
    if (!m_d->suppressSignals) {
        sliderValueChanged();
    }
}

void KisAspectRatioLocker::setBlockUpdateSignalOnDrag(bool value)
{
    m_d->blockUpdatesOnDrag = value;
}

void KisAspectRatioLocker::updateAspect()
{
    m_d->suppressSignals = true;
    slotAspectButtonChanged();
    m_d->suppressSignals = false;
}
