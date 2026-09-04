/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSHAPEFILLWRAPPER_H
#define KOSHAPEFILLWRAPPER_H

#include "kritaflake_export.h"
#include <PkScopedPointer.h>
#include <PkList.h>
#include <KoFlake.h>

class KUndo2Command;
class KoShape;
class PkColor;
class PkTransform;
class PkGradient;
class SvgMeshGradient;

class KRITAFLAKE_EXPORT KoShapeFillWrapper
{
public:
    KoShapeFillWrapper(KoShape *shape, KoFlake::FillVariant fillVariant);
    KoShapeFillWrapper(PkList<KoShape*> shapes, KoFlake::FillVariant fillVariant);

    ~KoShapeFillWrapper();

    bool isMixedFill() const;
    KoFlake::FillType type() const;

    PkColor color() const;
    const PkGradient* gradient() const;
    PkTransform gradientTransform() const;
    bool hasZeroLineWidth() const;
    const SvgMeshGradient* meshgradient() const;

    KUndo2Command* setColor(const PkColor &color);
    KUndo2Command* setLineWidth(const float &lineWidth);

    KUndo2Command* setGradient(const PkGradient *gradient, const PkTransform &transform);
    KUndo2Command* applyGradient(const PkGradient *gradient);
    KUndo2Command* applyGradientStopsOnly(const PkGradient *gradient);

    KUndo2Command* setMeshGradient(const SvgMeshGradient *gradient, const PkTransform &transform);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif // KOSHAPEFILLWRAPPER_H
