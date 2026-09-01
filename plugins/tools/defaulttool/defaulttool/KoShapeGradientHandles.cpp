/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoShapeGradientHandles.h"

#include <PkGradient.h>
#include <KoShape.h>
#include <KoGradientBackground.h>
#include <KoShapeBackgroundCommand.h>
#include <KoShapeFillWrapper.h>
#include <kis_assert.h>
#include "kis_algebra_2d.h"

KoShapeGradientHandles::KoShapeGradientHandles(KoFlake::FillVariant fillVariant, KoShape *shape)
    : m_fillVariant(fillVariant),
      m_shape(shape)
{
}

PkVector<KoShapeGradientHandles::Handle> KoShapeGradientHandles::handles() const {
    PkVector<Handle> result;

    const PkGradient *g = gradient();
    if (!g) return result;

    switch (g->type()) {
    case PkGradientEnums::LinearGradient: {
        const PkGradient *lgradient = g;
        result << Handle(Handle::LinearStart, lgradient->start());
        result << Handle(Handle::LinearEnd, lgradient->finalStop());
        break;
    }
    case PkGradientEnums::RadialGradient: {
        const PkGradient *rgradient = g;

        result << Handle(Handle::RadialCenter, rgradient->center());

        if (rgradient->center() != rgradient->focalPoint()) {
            result << Handle(Handle::RadialFocalPoint, rgradient->focalPoint());
        }

        result << Handle(Handle::RadialRadius,
                         rgradient->center() + PkPointF(rgradient->radius(), 0));
        break;
    }
    case PkGradientEnums::ConicalGradient:
        // not supported
        break;
    case PkGradientEnums::NoGradient:
        // not supported
        break;
    }

    if (g->coordinateMode() == PkGradientEnums::ObjectBoundingMode) {
        const PkRectF boundingRect = m_shape->outlineRect();
        const PkTransform gradientToUser(boundingRect.width(), 0, 0, boundingRect.height(),
                                        boundingRect.x(), boundingRect.y());
        const PkTransform t = gradientToUser * m_shape->absoluteTransformation();

        PkVector<Handle>::iterator it = result.begin();



        for (; it != result.end(); ++it) {
            it->pos = t.map(it->pos);
        }
    }

    return result;
}

PkGradientEnums::Type KoShapeGradientHandles::type() const
{
    const PkGradient *g = gradient();
    return g ? g->type() : PkGradientEnums::NoGradient;
}

KUndo2Command *KoShapeGradientHandles::moveGradientHandle(KoShapeGradientHandles::Handle::Type handleType, const PkPointF &absoluteOffset)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(handleType != Handle::None, 0);

    KoShapeFillWrapper wrapper(m_shape, m_fillVariant);
    const PkGradient *originalGradient = wrapper.gradient();
    PkTransform originalTransform = wrapper.gradientTransform();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(originalGradient, 0);

    PkScopedPointer<PkGradient> newGradient;

    switch (originalGradient->type()) {
    case PkGradientEnums::LinearGradient: {
        KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(handleType == Handle::LinearStart ||
                                             handleType == Handle::LinearEnd, 0);

        newGradient.reset(KoFlake::cloneGradient(originalGradient));
        PkGradient *lgradient = newGradient.data();

        if (handleType == Handle::LinearStart) {
            lgradient->setStart(getNewHandlePos(lgradient->start(), absoluteOffset, newGradient->coordinateMode()));
        } else if (handleType == Handle::LinearEnd) {
            lgradient->setFinalStop(getNewHandlePos(lgradient->finalStop(), absoluteOffset, newGradient->coordinateMode()));

        }
        break;
    }
    case PkGradientEnums::RadialGradient: {
        newGradient.reset(KoFlake::cloneGradient(originalGradient));
        PkGradient *rgradient = newGradient.data();

        if (handleType == Handle::RadialCenter) {
            rgradient->setCenter(getNewHandlePos(rgradient->center(), absoluteOffset, newGradient->coordinateMode()));
        } else if (handleType == Handle::RadialFocalPoint) {
            rgradient->setFocalPoint(getNewHandlePos(rgradient->focalPoint(), absoluteOffset, newGradient->coordinateMode()));
        } else if (handleType == Handle::RadialRadius) {
            PkPointF radiusPos = rgradient->center() + PkPointF(rgradient->radius(), 0);
            radiusPos = getNewHandlePos(radiusPos, absoluteOffset, newGradient->coordinateMode());
            rgradient->setRadius(radiusPos.x() - rgradient->center().x());
        }
        break;
    }
    case PkGradientEnums::ConicalGradient:
        // not supported
        break;
    case PkGradientEnums::NoGradient:
        // not supported
        break;
    }

    return wrapper.setGradient(newGradient.data(), originalTransform);
}

KoShapeGradientHandles::Handle KoShapeGradientHandles::getHandle(KoShapeGradientHandles::Handle::Type handleType)
{
    Handle result;

    for (const Handle &h : handles()) {
        if (h.type == handleType) {
            result = h;
            break;
        }
    }

    return result;
}

const PkGradient *KoShapeGradientHandles::gradient() const {
    KoShapeFillWrapper wrapper(m_shape, m_fillVariant);
    return wrapper.gradient();
}

PkPointF KoShapeGradientHandles::getNewHandlePos(const PkPointF &oldPos, const PkPointF &absoluteOffset, PkGradientEnums::CoordinateMode mode)
{
    const PkTransform offset = PkTransform::fromTranslate(absoluteOffset.x(), absoluteOffset.y());
    PkTransform localToAbsolute = m_shape->absoluteTransformation();
    PkTransform absoluteToLocal = localToAbsolute.inverted();

    if (mode == PkGradientEnums::ObjectBoundingMode) {
        const PkRectF rect = m_shape->outlineRect();
        const PkTransform gradientToUser = KisAlgebra2D::mapToRect(rect);
        localToAbsolute = gradientToUser * localToAbsolute;

        /// Some shapes may have zero-width/height, then inverted transform will not
        /// exist. Therefore we should use a special method for that.
        const PkTransform userToGradient = KisAlgebra2D::mapToRectInverse(rect);
        absoluteToLocal = absoluteToLocal * userToGradient;
    }

    return (localToAbsolute * offset * absoluteToLocal).map(oldPos);
}
