/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoShapeFillWrapper.h"
#include <KoGradientBridge.h>

#include <KoShape.h>
#include <PkList.h>
#include <QBrush>
#include <KoColorBackground.h>
#include <KoGradientBackground.h>
#include <KoPatternBackground.h>
#include <KoMeshGradientBackground.h>
#include <KoShapeStroke.h>
#include <KoShapeBackgroundCommand.h>
#include <KoShapeStrokeCommand.h>
#include <KoStopGradient.h>

#include "kis_assert.h"
#include "kis_debug.h"
#include "kis_global.h"

#include <KoFlakeUtils.h>

struct ShapeBackgroundFetchPolicy
{
    typedef KoFlake::FillType Type;

    typedef PkSharedPointer<KoShapeBackground> PointerType;
    static PointerType getBackground(KoShape *shape) {
        return shape->background();
    }
    static Type type(KoShape *shape) {
        PkSharedPointer<KoShapeBackground> background = shape->background();
        PkSharedPointer<KoColorBackground> colorBackground = pkSharedPointerDynamicCast<KoColorBackground>(background);
        PkSharedPointer<KoGradientBackground> gradientBackground = pkSharedPointerDynamicCast<KoGradientBackground>(background);
        PkSharedPointer<KoPatternBackground> patternBackground = pkSharedPointerDynamicCast<KoPatternBackground>(background);
        PkSharedPointer<KoMeshGradientBackground> meshgradientBackground = pkSharedPointerDynamicCast<KoMeshGradientBackground>(background);


        if(gradientBackground) {
            return Type::Gradient;
        }

        if (patternBackground) {
            return Type::Pattern;
        }

        if (colorBackground) {
            return Type::Solid;
        }

        if (meshgradientBackground) {
            return Type::MeshGradient;
        }

        return Type::None;
    }

    static PkColor color(KoShape *shape) {
        PkSharedPointer<KoColorBackground> colorBackground = pkSharedPointerDynamicCast<KoColorBackground>(shape->background());
        return colorBackground ? colorBackground->color() : PkColor();
    }

    static const PkGradient* gradient(KoShape *shape) {
        PkSharedPointer<KoGradientBackground> gradientBackground = pkSharedPointerDynamicCast<KoGradientBackground>(shape->background());
        return gradientBackground ? gradientBackground->gradient() : 0;
    }

    static PkTransform gradientTransform(KoShape *shape) {
        PkSharedPointer<KoGradientBackground> gradientBackground = pkSharedPointerDynamicCast<KoGradientBackground>(shape->background());
        return gradientBackground ? gradientBackground->transform() : PkTransform();
    }

    static const SvgMeshGradient* meshgradient(KoShape *shape) {
        PkSharedPointer<KoMeshGradientBackground> meshgradientBackground = pkSharedPointerDynamicCast<KoMeshGradientBackground>(shape->background());
        return meshgradientBackground ? meshgradientBackground->gradient() : nullptr;
    }

    static PkTransform meshgradientTransform(KoShape *shape) {
        PkSharedPointer<KoMeshGradientBackground> meshgradientBackground = pkSharedPointerDynamicCast<KoMeshGradientBackground>(shape->background());
        return meshgradientBackground ? meshgradientBackground->transform() : PkTransform();
    }

    static bool compareTo(PointerType p1, PointerType p2) {
        return p1->compareTo(p2.data());
    }
};

struct ShapeStrokeFillFetchPolicy
{
    typedef KoFlake::FillType Type;

    typedef KoShapeStrokeModelSP PointerType;
    static PointerType getBackground(KoShape *shape) {
        return shape->stroke();
    }
    static Type type(KoShape *shape) {
        KoShapeStrokeSP stroke = pkSharedPointerDynamicCast<KoShapeStroke>(shape->stroke());
        if (!stroke) return Type::None;

        // Pattern type not implemented yet, so that logic will have to be added here later
        if (stroke->lineBrush().gradient()) {
            return Type::Gradient;
        } else {

            // strokes without any width are none
            if (stroke->color().isValid() && stroke->lineWidth() != 0.0) {
                return Type::Solid;
            }

            return Type::None;
        }
    }

    static PkColor color(KoShape *shape) {
        KoShapeStrokeSP stroke = pkSharedPointerDynamicCast<KoShapeStroke>(shape->stroke());
        return stroke ? stroke->color() : PkColor();
    }

    static const PkGradient* gradient(KoShape *shape) {
        KoShapeStrokeSP stroke = pkSharedPointerDynamicCast<KoShapeStroke>(shape->stroke());
        return stroke ? toPkGradientPtr(stroke->lineBrush().gradient()) : 0;
    }

    static PkTransform gradientTransform(KoShape *shape) {
        KoShapeStrokeSP stroke = pkSharedPointerDynamicCast<KoShapeStroke>(shape->stroke());
        return stroke ? toPkTransform(stroke->lineBrush().transform()) : PkTransform();
    }

    static bool compareTo(PointerType p1, PointerType p2) {
        return p1->compareFillTo(p2.data());
    }
};


template <class Policy>
bool compareBackgrounds(const PkList<KoShape*> shapes)
{
    if (shapes.size() == 1) return true;

    typename Policy::PointerType bg =
        Policy::getBackground(shapes.first());

    Q_FOREACH (KoShape *shape, shapes) {
        if (
            !(
              (!bg && !Policy::getBackground(shape)) ||
              (bg && Policy::compareTo(bg, Policy::getBackground(shape)))
             )) {

            return false;
        }
    }

    return true;
}

/******************************************************************************/
/*             KoShapeFillWrapper::Private                                    */
/******************************************************************************/

struct KoShapeFillWrapper::Private
{
    PkList<KoShape*> shapes;
    KoFlake::FillVariant fillVariant= KoFlake::Fill;

    PkSharedPointer<KoShapeBackground> applyFillGradientStops(KoShape *shape, const PkGradient *srcQGradient);
    void applyFillGradientStops(KoShapeStrokeSP shapeStroke, const PkGradient *stopGradient);
};

PkSharedPointer<KoShapeBackground> KoShapeFillWrapper::Private::applyFillGradientStops(KoShape *shape, const PkGradient *stopGradient)
{
    PkGradientStops stops = stopGradient->stops();

    if (!shape || !stops.count()) {
        return PkSharedPointer<KoShapeBackground>();
    }

    KoGradientBackground *newGradient = 0;
    PkSharedPointer<KoGradientBackground> oldGradient = pkSharedPointerDynamicCast<KoGradientBackground>(shape->background());
    if (oldGradient) {
        // just copy the gradient and set the new stops
        PkGradient *g = KoFlake::mergeGradient(oldGradient->gradient(), stopGradient);
        newGradient = new KoGradientBackground(g);
        newGradient->setTransform(oldGradient->transform());
    }
    else {
        // No gradient yet, so create a new one.
        PkScopedPointer<PkGradient> fakeShapeGradient(new PkGradient(PkGradient::linear(PkPointF(0, 0), PkPointF(1, 1))));
        fakeShapeGradient->setCoordinateMode(PkGradientEnums::ObjectBoundingMode);

        PkGradient *g = KoFlake::mergeGradient(fakeShapeGradient.data(), stopGradient);
        newGradient = new KoGradientBackground(g);
    }
    return PkSharedPointer<KoGradientBackground>(newGradient);
}

void KoShapeFillWrapper::Private::applyFillGradientStops(KoShapeStrokeSP shapeStroke, const PkGradient *stopGradient)
{
    PkGradientStops stops = stopGradient->stops();
    if (!stops.count()) return;

    PkGradient fakeShapeGradient(PkGradient::linear(PkPointF(0, 0), PkPointF(1, 1)));
    fakeShapeGradient.setCoordinateMode(PkGradientEnums::ObjectBoundingMode);
    PkTransform gradientTransform;
    const PkGradient *shapeGradient = 0;

    {
        QBrush brush = shapeStroke->lineBrush();
        gradientTransform = toPkTransform(brush.transform());
        shapeGradient = brush.gradient() ? toPkGradientPtr(brush.gradient()) : &fakeShapeGradient;
    }

    {
        PkScopedPointer<PkGradient> g(KoFlake::mergeGradient(shapeGradient, stopGradient));
        QBrush newBrush(toQGradient(*g));
        newBrush.setTransform(toQTransform(gradientTransform));
        shapeStroke->setLineBrush(newBrush);
    }
}

/******************************************************************************/
/*             KoShapeFillWrapper                                             */
/******************************************************************************/

KoShapeFillWrapper::KoShapeFillWrapper(KoShape *shape, KoFlake::FillVariant fillVariant)
    : m_d(new Private())
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(shape);
    m_d->shapes << shape;
    m_d->fillVariant= fillVariant;
}


KoShapeFillWrapper::KoShapeFillWrapper(PkList<KoShape*> shapes, KoFlake::FillVariant fillVariant)
    : m_d(new Private())
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!shapes.isEmpty());
    m_d->shapes = shapes;
    m_d->fillVariant= fillVariant;
}

KoShapeFillWrapper::~KoShapeFillWrapper()
{
}

bool KoShapeFillWrapper::isMixedFill() const
{
    if (m_d->shapes.isEmpty()) return false;

    return m_d->fillVariant == KoFlake::Fill ?
        !compareBackgrounds<ShapeBackgroundFetchPolicy>(m_d->shapes) :
        !compareBackgrounds<ShapeStrokeFillFetchPolicy>(m_d->shapes);
}

KoFlake::FillType KoShapeFillWrapper::type() const
{
    if (m_d->shapes.isEmpty() || isMixedFill()) return KoFlake::None;

    KoShape *shape = m_d->shapes.first();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shape, KoFlake::None);

    KoFlake::FillType fillType;
    if (m_d->fillVariant == KoFlake::Fill) {
        // fill property of vector object
        fillType = ShapeBackgroundFetchPolicy::type(shape);
    } else {
        // stroke property of vector object
        fillType = ShapeStrokeFillFetchPolicy::type(shape);
    }

    return fillType;
}

PkColor KoShapeFillWrapper::color() const
{
    // this check guarantees that the shapes list is not empty and
    // the fill is not mixed!
    if (type() != KoFlake::Solid) return PkColor();

    KoShape *shape = m_d->shapes.first();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shape, PkColor());

    return m_d->fillVariant == KoFlake::Fill ?
        ShapeBackgroundFetchPolicy::color(shape) :
        ShapeStrokeFillFetchPolicy::color(shape);
}

const PkGradient* KoShapeFillWrapper::gradient() const
{
    // this check guarantees that the shapes list is not empty and
    // the fill is not mixed!
    if (type() != KoFlake::Gradient) return 0;

    KoShape *shape = m_d->shapes.first();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shape, 0);

    return m_d->fillVariant == KoFlake::Fill ?
        ShapeBackgroundFetchPolicy::gradient(shape) :
        ShapeStrokeFillFetchPolicy::gradient(shape);
}

PkTransform KoShapeFillWrapper::gradientTransform() const
{
    // this check guarantees that the shapes list is not empty and
    // the fill is not mixed!
    if (type() != KoFlake::Gradient) return PkTransform();

    KoShape *shape = m_d->shapes.first();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shape, PkTransform());

    return m_d->fillVariant == KoFlake::Fill ?
        ShapeBackgroundFetchPolicy::gradientTransform(shape) :
                ShapeStrokeFillFetchPolicy::gradientTransform(shape);
}

const SvgMeshGradient* KoShapeFillWrapper::meshgradient() const
{
    if (type() != KoFlake::MeshGradient) return nullptr;

    KoShape *shape = m_d->shapes.first();
    KIS_SAFE_ASSERT_RECOVER_RETURN_VALUE(shape, 0);

    return m_d->fillVariant == KoFlake::Fill ?
        ShapeBackgroundFetchPolicy::meshgradient(shape) :
        nullptr;
}

KUndo2Command *KoShapeFillWrapper::setColor(const PkColor &color)
{
    KUndo2Command *command = 0;

    if (m_d->fillVariant == KoFlake::Fill) {
         PkSharedPointer<KoShapeBackground> bg;

        if (color.isValid()) {
            bg = PkSharedPointer<KoColorBackground>(new KoColorBackground(color));
        }

        PkSharedPointer<KoShapeBackground> fill(bg);
        command = new KoShapeBackgroundCommand(toPkList(m_d->shapes), fill);
    } else {
        command = KoFlake::modifyShapesStrokes(m_d->shapes,
            [color] (KoShapeStrokeSP stroke) {
                stroke->setLineBrush(Qt::NoBrush);
                stroke->setColor(color);

            });
    }

    return command;
}

KUndo2Command *KoShapeFillWrapper::setLineWidth(const float &lineWidth)
{
    KUndo2Command *command = 0;

    command = KoFlake::modifyShapesStrokes(m_d->shapes, [lineWidth](KoShapeStrokeSP stroke) {
            stroke->setColor(PkColor(Pk::transparent));
            stroke->setLineWidth(lineWidth);

     });

   return command;
}


bool KoShapeFillWrapper::hasZeroLineWidth() const
{
        KoShape *shape = m_d->shapes.first();
        if (!shape) return false;
        if (m_d->fillVariant == KoFlake::Fill)  return false;

        // this check is useful to determine if
        KoShapeStrokeSP stroke = pkSharedPointerDynamicCast<KoShapeStroke>(shape->stroke());
        if (!stroke) return false;

        if ( stroke->lineWidth() == 0.0) {
            return true;
        }

        return false;
}


KUndo2Command *KoShapeFillWrapper::setGradient(const PkGradient *gradient, const PkTransform &transform)
{
    KUndo2Command *command = 0;

    if (m_d->fillVariant == KoFlake::Fill) {
        PkList<PkSharedPointer<KoShapeBackground>> newBackgrounds;

        foreach (KoShape *shape, m_d->shapes) {
            Q_UNUSED(shape);

            KoGradientBackground *newGradient = new KoGradientBackground(KoFlake::cloneGradient(gradient));
            newGradient->setTransform(transform);
            newBackgrounds << PkSharedPointer<KoGradientBackground>(newGradient);
        }

        command = new KoShapeBackgroundCommand(toPkList(m_d->shapes), toPkList(newBackgrounds));

    } else {
        command = KoFlake::modifyShapesStrokes(m_d->shapes,
            [gradient, transform] (KoShapeStrokeSP stroke) {
                QBrush newBrush(toQGradient(*gradient));
                newBrush.setTransform(toQTransform(transform));

                stroke->setLineBrush(newBrush);
                stroke->setColor(PkColor(Pk::transparent));
            });
    }

    return command;
}

KUndo2Command* KoShapeFillWrapper::applyGradient(const PkGradient *gradient)
{
    return setGradient(gradient, gradientTransform());
}

KUndo2Command* KoShapeFillWrapper::applyGradientStopsOnly(const PkGradient *gradient)
{
    KUndo2Command *command = 0;

    if (m_d->fillVariant == KoFlake::Fill) {
        PkList<PkSharedPointer<KoShapeBackground>> newBackgrounds;

        foreach (KoShape *shape, m_d->shapes) {
            newBackgrounds <<  m_d->applyFillGradientStops(shape, gradient);
        }

        command = new KoShapeBackgroundCommand(toPkList(m_d->shapes), toPkList(newBackgrounds));

    } else {
        command = KoFlake::modifyShapesStrokes(m_d->shapes,
            [this, gradient] (KoShapeStrokeSP stroke) {
                m_d->applyFillGradientStops(stroke, gradient);
            });
    }

    return command;
}

KUndo2Command* KoShapeFillWrapper::setMeshGradient(const SvgMeshGradient *gradient,
                                                   const PkTransform &transform)
{
    KUndo2Command *command = nullptr;
    if (m_d->fillVariant == KoFlake::Fill) {
        PkList<PkSharedPointer<KoShapeBackground>> newBackgrounds;

        for (const auto &shape: m_d->shapes) {
            Q_UNUSED(shape);
            KoMeshGradientBackground *newBackground =
                new KoMeshGradientBackground(gradient, transform);

            newBackgrounds << PkSharedPointer<KoMeshGradientBackground>(newBackground);
        }
        command = new KoShapeBackgroundCommand(toPkList(m_d->shapes), toPkList(newBackgrounds));
    }
    // TODO: for strokes!!
    return command;
}
