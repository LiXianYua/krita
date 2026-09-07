/*
 *  SPDX-FileCopyrightText: 2018 Anna Medonosova <anna.medonosova@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOGAMUTMASK_H
#define KOGAMUTMASK_H

#include <QPainter>
#include <PkString.h>
#include <PkVector.h>
#include <cmath>

#include <FlakeDebug.h>
#include <KoResource.h>
#include <KoShape.h>

#include <KisResourceTypes.h>
#include <PkString.h>
#include <pk/port/PkStream.h>

//class KoViewConverter;
class PkTransform;

class KoGamutMaskShape
{
public:
    KoGamutMaskShape(KoShape* shape);
    KoGamutMaskShape();
    ~KoGamutMaskShape();

    bool coordIsClear(const PkPointF& coord) const;
    PkPainterPath outline();
    void paint(QPainter &painter);
    void paintStroke(QPainter &painter);
    KoShape* koShape();

private:
    KoShape* m_maskShape {nullptr};
};


/**
 * @brief The resource type for gamut masks used by the artistic color selector
 */
class KRITAFLAKE_EXPORT KoGamutMask : public QObject, public KoResource
{
    Q_OBJECT

public:
    KoGamutMask(const PkString &filename);
    KoGamutMask();
    KoGamutMask(KoGamutMask *rhs);
    KoGamutMask(const KoGamutMask &rhs);
    KoGamutMask &operator=(const KoGamutMask &rhs) = delete;
    KoResourceSP clone() const override;
    ~KoGamutMask() override;

    bool coordIsClear(const PkPointF& coord, bool preview);
    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override;
    bool saveToDevice(PkStream* dev) const override;

    std::pair<PkString, PkString> resourceType() const override
    {
        return std::pair<PkString, PkString>(ResourceType::GamutMasks, PkString());
    }

    void paint(QPainter &painter, bool preview);
    void paintStroke(QPainter &painter, bool preview);

    PkTransform maskToViewTransform(qreal viewSize);
    PkTransform viewToMaskTransform(qreal viewSize);

    PkString title() const;
    void setTitle(PkString title);

    PkString description() const;
    void setDescription(PkString description);

    PkString defaultFileExtension() const override;

    int rotation();
    void setRotation(int rotation);

    PkSizeF maskSize();

    void setMaskShapes(PkList<KoShape*> shapes);
    void setPreviewMaskShapes(PkList<KoShape*> shapes);

    PkList<KoShape*> koShapes() const;

    void clearPreview();

private:
    void setMaskShapesToVector(PkList<KoShape*> shapes, PkVector<KoGamutMaskShape*>& targetVector);

    struct Private;
    Private* const d;
};

typedef PkSharedPointer<KoGamutMask> KoGamutMaskSP;

#endif // KOGAMUTMASK_H
