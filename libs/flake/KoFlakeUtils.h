/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOFLAKEUTILS_H
#define KOFLAKEUTILS_H

#include <KoShape.h>
#include <KoFlakeTypes.h>
#include <KoShapeStroke.h>

#include "kis_global.h"
#include "KoShapeStrokeCommand.h"

// toPkList 在下面 modifyShapesStrokes() 的模板体内是**非依赖名**（形参已是 PkList），
// 必须在模板定义点就可见——不能靠某个调用点碰巧先包了 bridge 头。见 PkFlakeBridge.h。
#include <PkFlakeBridge.h>


namespace KoFlake {

template <typename ModifyFunction>
    auto modifyShapesStrokes(PkList<KoShape*> shapes, ModifyFunction modifyFunction)
        -> decltype(modifyFunction(KoShapeStrokeSP()), (KUndo2Command*)(0))
    {
        if (shapes.isEmpty()) return 0;

        PkList<KoShapeStrokeModelSP> newStrokes;

        Q_FOREACH(KoShape *shape, shapes) {
            KoShapeStrokeSP shapeStroke = shape->stroke() ?
                pkSharedPointerDynamicCast<KoShapeStroke>(shape->stroke()) :
                KoShapeStrokeSP();

            KoShapeStrokeSP newStroke =
                PkSharedPointer<KoShapeStroke>(shapeStroke ?
                              new KoShapeStroke(*shapeStroke) :
                              new KoShapeStroke());

            modifyFunction(newStroke);

            newStrokes << newStroke;
        }

        return new KoShapeStrokeCommand(toPkList(shapes), toPkList(newStrokes));
}

template <class Policy>
bool compareShapePropertiesEqual(const PkList<KoShape*> shapes, const Policy &policy)
{
    if (shapes.size() == 1) return true;

    typename Policy::PointerType bg =
            policy.getProperty(shapes.first());

    Q_FOREACH (KoShape *shape, shapes) {
        typename Policy::PointerType otherBg = policy.getProperty(shape);

        if (
            !(
                (!bg && !otherBg) ||
                (bg && otherBg && policy.compareTo(bg, otherBg))
                )) {

            return false;
        }
    }

    return true;
}

template <class Policy>
bool compareShapePropertiesEqual(const PkList<KoShape*> shapes)
{
    return compareShapePropertiesEqual<Policy>(shapes, Policy());
}

}

#endif // KOFLAKEUTILS_H

