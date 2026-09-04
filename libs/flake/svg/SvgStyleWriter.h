/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2002 Lars Siebold <khandha5@gmx.net>
   SPDX-FileCopyrightText: 2002 Werner Trobin <trobin@kde.org>
   SPDX-FileCopyrightText: 2002 Lennart Kudling <kudling@kde.org>
   SPDX-FileCopyrightText: 2002-2003, 2005 Rob Buis <buis@kde.org>
   SPDX-FileCopyrightText: 2005 Boudewijn Rempt <boud@valdyas.org>
   SPDX-FileCopyrightText: 2005 Raphael Langerhorst <raphael.langerhorst@kdemail.net>
   SPDX-FileCopyrightText: 2005 Thomas Zander <zander@kde.org>
   SPDX-FileCopyrightText: 2005, 2008 Jan Hambrecht <jaham@gmx.net>
   SPDX-FileCopyrightText: 2006 Inge Wallin <inge@lysator.liu.se>
   SPDX-FileCopyrightText: 2006 Laurent Montel <montel@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#ifndef SVGSTYLEWRITER_H
#define SVGSTYLEWRITER_H

#include "kritaflake_export.h"
#include "KoFlakeTypes.h"
#include <PkGradient.h>
#include <PkSharedPointer.h>
#include <KoShape.h>

class SvgSavingContext;
class KoShape;
class KoPatternBackground;
class KoVectorPatternBackground;
class KoShapeBackground;
class PkTransform;
class PkGradient;
class SvgMeshGradient;

/// Helper class to save svg styles
class KRITAFLAKE_EXPORT SvgStyleWriter
{
public:
    /// Saves the style of the specified shape
    static void saveSvgStyle(KoShape *shape, SvgSavingContext &context);

    /// Saves only stroke, fill and transparency of the shape
    static void saveSvgBasicStyle(const bool isVisible, const qreal transparency, const PkVector<KoShape::PaintOrder> paintOrder, bool inheritPaintorder, SvgSavingContext &context, bool textShape = false);

    /// Saves fill style of specified shape
    static void saveSvgFill(PkSharedPointer<KoShapeBackground> background, const bool fillRuleEvenOdd, const PkRectF outlineRect, const PkSizeF size, const PkTransform absoluteTransform, SvgSavingContext &context);
    /// Saves stroke style of specified shape
    static void saveSvgStroke(KoShapeStrokeModelSP, SvgSavingContext &context);

    // embed the given shape, returns an id to refer to.
    static PkString embedShape(const KoShape *shape, SvgSavingContext &context);

    // Save title and desc elements.
    static void saveMetadata(const KoShape *shape, SvgSavingContext &context);

protected:
    /// Saves clipping of specified shape
    static void saveSvgClipping(KoShape *shape, SvgSavingContext &context);
    /// Saves masking of specified shape
    static void saveSvgMasking(KoShape *shape, SvgSavingContext &context);
    /// Saves markers of the path shape if present
    static void saveSvgMarkers(KoShape *shape, SvgSavingContext &context);
    /// Saves gradient color stops
    static void saveSvgColorStops(const PkGradientStops &colorStops, SvgSavingContext &context);
    /// Saves gradient
    static PkString saveSvgGradient(const PkGradient *gradient, const PkTransform &gradientTransform, SvgSavingContext &context);
    static PkString saveSvgMeshGradient(SvgMeshGradient* gradient, const PkTransform &transform, SvgSavingContext &context);
    /// Saves pattern
    static PkString saveSvgPattern(PkSharedPointer<KoPatternBackground> pattern, const PkSizeF shapeSize, const PkTransform absoluteTransform, SvgSavingContext &context);
    static PkString saveSvgVectorPattern(PkSharedPointer<KoVectorPatternBackground> pattern, const PkRectF outlineRect, SvgSavingContext &context);
};

#endif // SVGSTYLEWRITER_H
