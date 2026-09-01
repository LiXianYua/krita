/*
 *  SPDX-FileCopyrightText: 2017 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOSHAPEGRADIENTHANDLES_H
#define KOSHAPEGRADIENTHANDLES_H

#include <PkPoint.h>
#include <PkGradient.h>
#include <KoFlake.h>

class KoShape;
class KoViewConverter;
class KUndo2Command;

class KoShapeGradientHandles
{
public:
    struct Handle {
        enum Type {
            None,
            LinearStart,
            LinearEnd,
            RadialCenter,
            RadialRadius,
            RadialFocalPoint
        };

        Handle() : type(None) {}
        Handle(Type t, const PkPointF &p) : type(t), pos(p) {}

        Type type;
        PkPointF pos;
    };

public:
    KoShapeGradientHandles(KoFlake::FillVariant fillVariant, KoShape *shape);
    PkVector<Handle> handles() const;
    PkGradientEnums::Type type() const;

    KUndo2Command* moveGradientHandle(Handle::Type handleType, const PkPointF &absoluteOffset);
    Handle getHandle(Handle::Type handleType);



private:
    const PkGradient* gradient() const;
    PkPointF getNewHandlePos(const PkPointF &oldPos, const PkPointF &absoluteOffset, PkGradientEnums::CoordinateMode mode);

private:
    KoFlake::FillVariant m_fillVariant;
    KoShape *m_shape;
};

#endif // KOSHAPEGRADIENTHANDLES_H
