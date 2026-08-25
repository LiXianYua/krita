/*
 *  SPDX-FileCopyrightText: 2004 Michael Thaler <michael.thaler@physik.tu-muenchen.de>
 *  SPDX-FileCopyrightText: 2005 C. Boemann <cbo@boemann.dk>
 *  SPDX-FileCopyrightText: 2013 Juan Palacios <jpalaciosdev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_FILTER_STRATEGY_H_
#define KIS_FILTER_STRATEGY_H_

#include "KoGenericRegistry.h"
#include "KoID.h"
#include "kritaimage_export.h"
#include <PkSize.h>

class KRITAIMAGE_EXPORT KisFilterStrategy
{
public:
    KisFilterStrategy(KoID id) : m_id(id) {}
    virtual ~KisFilterStrategy() {  }

    PkString id() {
        return m_id.id();
    }
    PkString name() {
        return m_id.name();
    }
    virtual qreal valueAt(qreal t, qreal weightsPositionScale) const {
        Q_UNUSED(t);
        Q_UNUSED(weightsPositionScale);
        return 0;
    }
    virtual qint32 intValueAt(qint32 t, qreal weightsPositionScale) const {
        return qint32(255*valueAt(t / 256.0, weightsPositionScale));
    }
    virtual qreal support(qreal weightsPositionScale) {
        Q_UNUSED(weightsPositionScale);
        return supportVal;
    }
    virtual qint32 intSupport(qreal weightsPositionScale) {
        Q_UNUSED(weightsPositionScale);
        return intSupportVal;
    }
    virtual PkString description() {
        return PkString();
    }

protected:
    qreal supportVal {0.0};
    qint32 intSupportVal {0};
    KoID m_id;
};

class KRITAIMAGE_EXPORT KisHermiteFilterStrategy : public KisFilterStrategy
{
public:
    KisHermiteFilterStrategy() : KisFilterStrategy(KoID("Hermite", PkString("Hermite"))) {
        supportVal = 1.0; intSupportVal = 256;
    }
    ~KisHermiteFilterStrategy() override {}

    qint32 intValueAt(qint32 t, qreal weightsPositionScale) const override;
    qreal valueAt(qreal t, qreal weightsPositionScale) const override;
};

class KRITAIMAGE_EXPORT KisBicubicFilterStrategy : public KisFilterStrategy
{
public:
    KisBicubicFilterStrategy() : KisFilterStrategy(KoID("Bicubic", PkString("Bicubic"))) {
        supportVal = 2.0; intSupportVal = 512;
    }
    ~KisBicubicFilterStrategy() override {}

    PkString description() override {
        return PkString("Adds pixels using the color of surrounding pixels. Produces smoother tonal gradations than Bilinear.");
    }

    qint32 intValueAt(qint32 t, qreal weightsPositionScale) const override;
};
class KRITAIMAGE_EXPORT KisBoxFilterStrategy : public KisFilterStrategy
{
public:
    KisBoxFilterStrategy() : KisFilterStrategy(KoID("NearestNeighbor", PkString("Nearest Neighbor"))) {
        // 0.5 and 128, but with a bit of margin to ensure the correct pixel will be used
        // even in case of calculation errors
        supportVal = 0.51; intSupportVal = 129;
    }
    ~KisBoxFilterStrategy() override {}

    PkString description() override {
        return PkString("Replicate pixels in the image. Preserves all the original detail, but can produce jagged effects.");
    }

    virtual qreal support(qreal weightsPositionScale) override;
    virtual qint32 intSupport(qreal weightsPositionScale) override;


    qint32 intValueAt(qint32 t, qreal weightsPositionScale) const override;
    qreal valueAt(qreal t, qreal weightsPositionScale) const override;
};

class KRITAIMAGE_EXPORT KisBilinearFilterStrategy : public KisFilterStrategy
{
public:
    KisBilinearFilterStrategy() : KisFilterStrategy(KoID("Bilinear", PkString("Bilinear"))) {
        supportVal = 1.0; intSupportVal = 256;
    }
    ~KisBilinearFilterStrategy() override {}

    PkString description() override {
        return PkString("Adds pixels averaging the color values of surrounding pixels. Produces medium quality results when the image is scaled from half to two times the original size.");
    }

    qint32 intValueAt(qint32 t, qreal weightsPositionScale) const override;
    qreal valueAt(qreal t, qreal weightsPositionScale) const override;
};

class KRITAIMAGE_EXPORT KisBellFilterStrategy : public KisFilterStrategy
{
public:
    KisBellFilterStrategy() : KisFilterStrategy(KoID("Bell", PkString("Bell"))) {
        supportVal = 1.5; intSupportVal = 128 + 256;
    }
    ~KisBellFilterStrategy() override {}

    qreal valueAt(qreal t, qreal weightsPositionScale) const override;
};

class KRITAIMAGE_EXPORT KisBSplineFilterStrategy : public KisFilterStrategy
{
public:
    KisBSplineFilterStrategy() : KisFilterStrategy(KoID("BSpline", PkString("BSpline"))) {
        supportVal = 2.0; intSupportVal = 512;
    }
    ~KisBSplineFilterStrategy() override {}

    qreal valueAt(qreal t, qreal weightsPositionScale) const override;
};

class KRITAIMAGE_EXPORT KisLanczos3FilterStrategy : public KisFilterStrategy
{
public:
    KisLanczos3FilterStrategy() : KisFilterStrategy(KoID("Lanczos3", PkString("Lanczos3"))) {
        supportVal = 3.0; intSupportVal = 768;
    }
    ~KisLanczos3FilterStrategy() override {}

    PkString description() override {
        return PkString("Offers similar results than Bicubic, but maybe a little bit sharper. Can produce light and dark halos along strong edges.");
    }

    qreal valueAt(qreal t, qreal weightsPositionScale) const override;
private:
    qreal sinc(qreal x) const;
};

class KRITAIMAGE_EXPORT  KisMitchellFilterStrategy : public KisFilterStrategy
{
public:
    KisMitchellFilterStrategy() : KisFilterStrategy(KoID("Mitchell", PkString("Mitchell"))) {
        supportVal = 2.0; intSupportVal = 256;
    }
    ~KisMitchellFilterStrategy() override {}

    qreal valueAt(qreal t, qreal weightsPositionScale) const override;
};

class KRITAIMAGE_EXPORT KisFilterStrategyRegistry : public KoGenericRegistry<KisFilterStrategy *>
{

public:

    KisFilterStrategyRegistry();
    ~KisFilterStrategyRegistry() override;
    static KisFilterStrategyRegistry* instance();

    /**
     * This function return a list of all the keys in KoID format by using the name() method
     * on the objects stored in the registry.
     */
    PkList<KoID> listKeys() const;

    /**
     * This function return a string formatted in HTML that contains the descriptions of all objects
     * (with a non empty description) stored in the registry.
     */
    PkString formattedDescriptions() const;

    /**
     * Try to select an appropriate image filtering strategy based on original and desired parameters.
     */
    KisFilterStrategy* autoFilterStrategy(PkSize originalSize, PkSize desiredSize) const;

private:

    KisFilterStrategyRegistry(const KisFilterStrategyRegistry&);
    KisFilterStrategyRegistry operator=(const KisFilterStrategyRegistry&);

};

#endif // KIS_FILTER_STRATEGY_H_
