/*
 *  SPDX-FileCopyrightText: 2010 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2010 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_DEFAULT_BOUNDS_H
#define KIS_DEFAULT_BOUNDS_H

#include <PkRect.h>
#include <PkScopedPointer.h>

#include "kis_types.h"
#include "kis_default_bounds_base.h"

class KisDefaultBounds;
class KisSelectionDefaultBounds;
class KisSelectionEmptyBounds;
class KisWrapAroundBoundsWrapper;
typedef KisSharedPtr<KisDefaultBounds> KisDefaultBoundsSP;
typedef KisSharedPtr<KisSelectionDefaultBounds> KisSelectionDefaultBoundsSP;
typedef KisSharedPtr<KisSelectionEmptyBounds> KisSelectionEmptyBoundsSP;
typedef KisSharedPtr<KisWrapAroundBoundsWrapper> KisWrapAroundBoundsWrapperSP;

class KRITAIMAGE_EXPORT KisDefaultBounds :  public KisDefaultBoundsBase
{
public:
    KisDefaultBounds();
    KisDefaultBounds(KisImageWSP image);
    ~KisDefaultBounds() override;

    PkRect bounds() const override;
    bool wrapAroundMode() const override;
    WrapAroundAxis wrapAroundModeAxis() const override;
    int currentLevelOfDetail() const override;
    int currentTime() const override;
    bool externalFrameActive() const override;
    void * sourceCookie() const override;

protected:
    friend class KisPaintDeviceTest;
    static const PkRect infiniteRect;

private:
    KisDefaultBounds(const KisDefaultBounds &) = delete;
    KisDefaultBounds &operator=(const KisDefaultBounds &) = delete;

    struct Private;
    Private * const m_d;
};

/**
 * For a selection, the default bounds are defined not by the image, but
 * by the device the selection is attached to, i.e. the selection with
 * non-transparent default pixel should cover the entire parent paint
 * device, not just the image bounds.
 *
 * KisSelectionDefaultBoundsBase is a base class for selection-targeted
 * default bounds objects. The descendants can redefine how exactly "the
 * parent paint device" is fetched.
 */
class KRITAIMAGE_EXPORT KisSelectionDefaultBoundsBase : public KisDefaultBoundsBase
{
public:
    KisSelectionDefaultBoundsBase();
    ~KisSelectionDefaultBoundsBase() override;

    PkRect bounds() const override;
    PkRect imageBorderRect() const override;
    bool wrapAroundMode() const override;
    WrapAroundAxis wrapAroundModeAxis() const override;
    int currentLevelOfDetail() const override;
    int currentTime() const override;
    bool externalFrameActive() const override;
    void * sourceCookie() const override;

protected:
    /**
     * Return the actual paint device the selection is attached to
     */
    virtual KisPaintDeviceSP parentPaintDevice() const = 0;

private:
    KisSelectionDefaultBoundsBase(const KisSelectionDefaultBoundsBase &) = delete;
    KisSelectionDefaultBoundsBase &operator=(const KisSelectionDefaultBoundsBase &) = delete;
};

/**
 * KisSelectionDefaultBounds directly attaches a selection to a paint device
 *
 * This object is mostly used in immediate actions, like filter application,
 * where we don't have to store the link to the paint device for a long time.
 */
class KRITAIMAGE_EXPORT KisSelectionDefaultBounds : public KisSelectionDefaultBoundsBase
{
public:
    KisSelectionDefaultBounds(KisPaintDeviceSP parentPaintDevice);
    ~KisSelectionDefaultBounds() override;

protected:
    virtual KisPaintDeviceSP parentPaintDevice() const override;

private:
    KisSelectionDefaultBounds(const KisSelectionDefaultBounds &) = delete;
    KisSelectionDefaultBounds &operator=(const KisSelectionDefaultBounds &) = delete;
    KisPaintDeviceWSP m_paintDevice;
};

/**
 * KisMaskDefaultBounds is used to attach a selection of a mask to the origin()
 * of the parent layer. We cannot connect it directly to parent->origin(), because
 * in some cases the origin() of the parent layer may change randomly
 * (e.g. the origin() of a group layer may change due to the oblige child
 * mechanism).
 */
class KRITAIMAGE_EXPORT KisMaskDefaultBounds : public KisSelectionDefaultBoundsBase
{
public:
    KisMaskDefaultBounds(KisNodeSP parentNode);
    ~KisMaskDefaultBounds() override;

protected:
    virtual KisPaintDeviceSP parentPaintDevice() const override;

private:
    KisMaskDefaultBounds(const KisMaskDefaultBounds &) = delete;
    KisMaskDefaultBounds &operator=(const KisMaskDefaultBounds &) = delete;
    KisNodeWSP m_parentNode;
};

/**
 * This is a stub default bounds object that returns null rect
 * as the default bounds (in contrast to the infinite rect
 * returned by the base class).
 */
class KRITAIMAGE_EXPORT KisSelectionEmptyBounds : public KisDefaultBounds
{
public:
    KisSelectionEmptyBounds();
    KisSelectionEmptyBounds(KisImageWSP image);
    ~KisSelectionEmptyBounds() override;
    PkRect bounds() const override;
};

/**
 * @brief The KisWrapAroundBoundsWrapper class
 * wrapper around a KisDefaultBoundsBaseSP to enable
 * wraparound. Used for patterns.
 */
class KRITAIMAGE_EXPORT KisWrapAroundBoundsWrapper :  public KisDefaultBoundsBase
{
public:
    KisWrapAroundBoundsWrapper(KisDefaultBoundsBaseSP base, PkRect bounds);
    ~KisWrapAroundBoundsWrapper() override;

    PkRect bounds() const override;
    bool wrapAroundMode() const override;
    WrapAroundAxis wrapAroundModeAxis() const override;
    int currentLevelOfDetail() const override;
    int currentTime() const override;
    bool externalFrameActive() const override;
    void * sourceCookie() const override;

protected:
    friend class KisPaintDeviceTest;

private:
    KisWrapAroundBoundsWrapper(const KisWrapAroundBoundsWrapper &) = delete;
    KisWrapAroundBoundsWrapper &operator=(const KisWrapAroundBoundsWrapper &) = delete;

    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif // KIS_DEFAULT_BOUNDS_H
