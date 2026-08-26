/*
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2005 Bart Coppens <kde@bartcoppens.be>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_IMAGEPIPE_BRUSH_
#define KIS_IMAGEPIPE_BRUSH_

#include <PkMap.h>
#include <PkString.h>

#include <KoResource.h>
#include "kis_gbr_brush.h"
#include "kis_global.h"

class KisPipeBrushParasite;

/**
 * Velocity won't be supported, atm Tilt isn't either,
 * but have chances of implementation
 */
namespace KisParasite
{
enum SelectionMode {
    Constant,
    Incremental,
    Angular,
    Velocity,
    Random,
    Pressure,
    TiltX,
    TiltY
};
}

class BRUSH_EXPORT KisImagePipeBrush : public KisGbrBrush
{

public:
    KisImagePipeBrush(const PkString& filename);
    /**
     * Specialized constructor that makes a new pipe brush from a sequence of samesize
     * devices. The fact that it's a vector of a vector, is to support multidimensional
     * brushes (not yet supported!) */
    KisImagePipeBrush(const PkString& name, int w, int h,
                      PkVector< PkVector<KisPaintDevice*> > devices,
                      PkVector<KisParasite::SelectionMode> modes);

    ~KisImagePipeBrush() override;

    /// Will call KisBrush's saveToDevice as well
    KisImagePipeBrush(const KisImagePipeBrush& rhs);

    KisImagePipeBrush &operator=(const KisImagePipeBrush &rhs) = delete;

    KoResourceSP clone() const override;

    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override;
    bool saveToDevice(PkStream* dev) const override;

    /**
     * @return the next image in the pipe.
    */
    KisFixedPaintDeviceSP paintDevice(const KoColorSpace * colorSpace,
            KisDabShape const&,
            const KisPaintInformation& info,
            double subPixelX = 0, double subPixelY = 0) const override;

    void setAdjustmentMidPoint(quint8 value) override;
    void setBrightnessAdjustment(qreal value) override;
    void setContrastAdjustment(qreal value) override;
    void setAutoAdjustMidPoint(bool value) override;

    PkString parasiteSelection(); // returns random, constant, etc

    KisOptimizedBrushOutline outline(bool forcePreciseOutline = false) const override;

    bool canPaintFor(const KisPaintInformation& info) override;

    void makeMaskImage(bool preserveAlpha) override;


    PkString defaultFileExtension() const override;
    void setAngle(qreal _angle) override;
    void setScale(qreal _scale) override;
    void setSpacing(double _spacing) override;

    quint32 brushIndex() const override;
    qint32 maskWidth(KisDabShape const&, double subPixelX, double subPixelY, const KisPaintInformation& info) const override;
    qint32 maskHeight(KisDabShape const&, double subPixelX, double subPixelY, const KisPaintInformation& info) const override;

    void notifyStrokeStarted() override;
    void prepareForSeqNo(const KisPaintInformation& info, int seqNo) override;

    void generateMaskAndApplyMaskOrCreateDab(KisFixedPaintDeviceSP dst, KisBrush::ColoringInformation* coloringInformation,
            KisDabShape const&,
            const KisPaintInformation& info,
            double subPixelX = 0, double subPixelY = 0, 
            qreal softnessFactor = DEFAULT_SOFTNESS_FACTOR, qreal lightnessStrength = DEFAULT_LIGHTNESS_STRENGTH) const override;

    void notifyBrushIsGoingToBeClonedForStroke() override;

    PkVector<KisGbrBrushSP> brushes() const;

    const KisPipeBrushParasite &parasite() const;

    void setParasite(const KisPipeBrushParasite& parasite);
    void setDevices(PkVector< PkVector<KisPaintDevice*> > devices, int w, int h);

    void coldInitBrush() override;

protected:
    virtual void setBrushApplication(enumBrushApplication brushApplication) override;
    virtual void setGradient(KoAbstractGradientSP gradient) override;
    /// Will call KisBrush's saveToDevice as well

private:
    friend class KisImagePipeBrushTest;

    KisGbrBrushSP testingGetCurrentBrush(const KisPaintInformation& info) const;
    void testingSelectNextBrush(const KisPaintInformation& info) const;

    bool initFromData(const PkByteArray &data);

    PkString parasiteSelectionString; // incremental, random, etc.

private:
    struct Private;
    Private * const d;
};

typedef PkSharedPointer<KisImagePipeBrush> KisImagePipeBrushSP;

#endif // KIS_IMAGEPIPE_BRUSH_
