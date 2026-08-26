/*
 *  SPDX-FileCopyrightText: 2018 Anna Medonosova <anna.medonosova@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOGAMUTMASK_H
#define KOGAMUTMASK_H

#include <QPainter>
#include <QString>
#include <QVector>
#include <cmath>

#include <FlakeDebug.h>
#include <KoResource.h>
#include <KoShape.h>

#include <KisResourceTypes.h>
#include <PkString.h>
#include <pk/port/PkStream.h>

//class KoViewConverter;
class QTransform;

class KoGamutMaskShape
{
public:
    KoGamutMaskShape(KoShape* shape);
    KoGamutMaskShape();
    ~KoGamutMaskShape();

    bool coordIsClear(const QPointF& coord) const;
    QPainterPath outline();
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
    KoGamutMask(const QString &filename);
    // 过渡期适配：stripped KisResourceLoader<T>::create() 用 PkString 构造资源（KisResourceLoader.h），
    // real-Qt-first 的 KoGamutMask.cpp 只收 QString。加 PkString 版（内部 KoResource 直接收 PkString），
    // 语义与 QString 版等价（QString 版也是 toPkString 后存 PkString）。flake 剥完（Q* 归零）后随 KisResourceLoader 一起删。
    KoGamutMask(const PkString &filename);
    KoGamutMask();
    KoGamutMask(KoGamutMask *rhs);
    KoGamutMask(const KoGamutMask &rhs);
    KoGamutMask &operator=(const KoGamutMask &rhs) = delete;
    KoResourceSP clone() const override;
    ~KoGamutMask() override;

    bool coordIsClear(const QPointF& coord, bool preview);
    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override;
    bool saveToDevice(PkStream* dev) const override;

    std::pair<PkString, PkString> resourceType() const override
    {
        return std::pair<PkString, PkString>(ResourceType::GamutMasks, PkString());
    }

    void paint(QPainter &painter, bool preview);
    void paintStroke(QPainter &painter, bool preview);

    QTransform maskToViewTransform(qreal viewSize);
    QTransform viewToMaskTransform(qreal viewSize);

    QString title() const;
    void setTitle(QString title);

    QString description() const;
    void setDescription(QString description);

    PkString defaultFileExtension() const override;

    int rotation();
    void setRotation(int rotation);

    QSizeF maskSize();

    void setMaskShapes(QList<KoShape*> shapes);   
    void setPreviewMaskShapes(QList<KoShape*> shapes);

    QList<KoShape*> koShapes() const;

    void clearPreview();

private:
    void setMaskShapesToVector(QList<KoShape*> shapes, QVector<KoGamutMaskShape*>& targetVector);

    struct Private;
    Private* const d;
};

typedef QSharedPointer<KoGamutMask> KoGamutMaskSP;

#endif // KOGAMUTMASK_H
