/*
 * SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ASPECT_RATIO_LOCKER_H
#define __KIS_ASPECT_RATIO_LOCKER_H

#include <functional>

#include <PkObject.h>
#include <PkScopedPointer.h>
#include <PkSignalCompat.h>

#include <kritacanvas_export.h>

class KRITACANVAS_EXPORT KisAspectRatioValueControl
{
public:
    virtual ~KisAspectRatioValueControl() = default;
    virtual qreal value() const = 0;
    virtual void setValue(qreal value) = 0;
    virtual void setValueChangedCallback(std::function<void()> callback) = 0;
};

class KRITACANVAS_EXPORT KisAspectRatioToggleControl
{
public:
    virtual ~KisAspectRatioToggleControl() = default;
    virtual bool isChecked() const = 0;
    virtual void setToggledCallback(std::function<void(bool)> callback) = 0;
};

/** Keeps two numeric controls at a fixed ratio and normalizes notifications. */
class KRITACANVAS_EXPORT KisAspectRatioLocker : public PkObject
{
public:
    explicit KisAspectRatioLocker(PkObject *parent = nullptr);
    ~KisAspectRatioLocker() override;

    void connectSpinBoxes(KisAspectRatioValueControl *spinOne,
                          KisAspectRatioValueControl *spinTwo,
                          KisAspectRatioToggleControl *aspectButton);

    void setBlockUpdateSignalOnDrag(bool block);
    void updateAspect();
    void slotSpinDraggingFinished();

signals:
    void sliderValueChanged();
    void aspectButtonChanged();
    void aspectButtonToggled(bool value);

private:
    void slotSpinOneChanged();
    void slotSpinTwoChanged();
    void slotAspectButtonChanged();

    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_ASPECT_RATIO_LOCKER_H */
