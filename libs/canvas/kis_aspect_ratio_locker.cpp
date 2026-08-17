/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_aspect_ratio_locker.h"

#include <QSpinBox>
#include <QDoubleSpinBox>
#include <QAbstractButton>

#include "kis_signals_blocker.h"


struct SliderWrapper
{
    template <class Slider>
    SliderWrapper(Slider *slider)
        : m_slider(QVariant::fromValue(slider)),
          m_object(slider) {}

    void setValue(qreal value) {
        if (auto *slider = m_slider.value<QDoubleSpinBox*>()) {
            slider->setValue(value);

        } else if (auto *slider = m_slider.value<QSpinBox*>()) {
            slider->setValue(qRound(value));

        }
    }

    qreal value() const {
        qreal result = 0.0;

        if (auto *slider = m_slider.value<QDoubleSpinBox*>()) {
            result = slider->value();

        } else if (auto *slider = m_slider.value<QSpinBox*>()) {
            result = slider->value();

        }

        return result;
    }

    QObject* object() const {
        return m_object;
    }

private:
    QVariant m_slider;
    QObject *m_object;
};

struct KisAspectRatioLocker::Private
{
    QScopedPointer<SliderWrapper> spinOne;
    QScopedPointer<SliderWrapper> spinTwo;
    QAbstractButton *aspectButton = 0;

    qreal aspectRatio = 1.0;
    bool blockUpdatesOnDrag = false;
};


KisAspectRatioLocker::KisAspectRatioLocker(QObject *parent)
    : QObject(parent),
      m_d(new Private)
{
}

KisAspectRatioLocker::~KisAspectRatioLocker()
{
}

template <class SpinBoxType>
void KisAspectRatioLocker::connectSpinBoxes(SpinBoxType *spinOne, SpinBoxType *spinTwo, QAbstractButton *aspectButton)
{
    m_d->spinOne.reset(new SliderWrapper(spinOne));
    m_d->spinTwo.reset(new SliderWrapper(spinTwo));
    m_d->aspectButton = aspectButton;

    if (QVariant::fromValue(spinOne->value()).type() == QMetaType::Double) {
        connect(spinOne, SIGNAL(valueChanged(qreal)), SLOT(slotSpinOneChanged()));
        connect(spinTwo, SIGNAL(valueChanged(qreal)), SLOT(slotSpinTwoChanged()));
    } else {
        connect(spinOne, SIGNAL(valueChanged(int)), SLOT(slotSpinOneChanged()));
        connect(spinTwo, SIGNAL(valueChanged(int)), SLOT(slotSpinTwoChanged()));
    }

    connect(m_d->aspectButton, SIGNAL(toggled(bool)), SLOT(slotAspectButtonChanged()));
    slotAspectButtonChanged();
}


template KRITACANVAS_EXPORT void KisAspectRatioLocker::connectSpinBoxes(QSpinBox *spinOne, QSpinBox *spinTwo, QAbstractButton *aspectButton);
template KRITACANVAS_EXPORT void KisAspectRatioLocker::connectSpinBoxes(QDoubleSpinBox *spinOne, QDoubleSpinBox *spinTwo, QAbstractButton *aspectButton);

void KisAspectRatioLocker::slotSpinOneChanged()
{
    if (m_d->aspectButton->isChecked()) {
        KisSignalsBlocker b(m_d->spinTwo->object());
        m_d->spinTwo->setValue(m_d->aspectRatio * m_d->spinOne->value());
    }

    Q_EMIT sliderValueChanged();
}

void KisAspectRatioLocker::slotSpinTwoChanged()
{
    if (m_d->aspectButton->isChecked()) {
        KisSignalsBlocker b(m_d->spinOne->object());
        m_d->spinOne->setValue(m_d->spinTwo->value() / m_d->aspectRatio);
    }

    Q_EMIT sliderValueChanged();
}

void KisAspectRatioLocker::slotAspectButtonChanged()
{
    if (m_d->aspectButton->isChecked() &&
        m_d->spinTwo->value() > 0 &&
        m_d->spinOne->value() > 0) {
        m_d->aspectRatio = qreal(m_d->spinTwo->value()) / m_d->spinOne->value();
    } else {
        m_d->aspectRatio = 1.0;
    }

    Q_EMIT aspectButtonChanged();
    Q_EMIT aspectButtonToggled(m_d->aspectButton->isChecked());
}

void KisAspectRatioLocker::slotSpinDraggingFinished()
{
    Q_EMIT sliderValueChanged();
}

void KisAspectRatioLocker::setBlockUpdateSignalOnDrag(bool value)
{
    m_d->blockUpdatesOnDrag = value;
}

void KisAspectRatioLocker::updateAspect()
{
    KisSignalsBlocker b(this);
    slotAspectButtonChanged();
}