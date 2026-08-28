/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */
#include <KoShapeRegistry.h>
#include <KoShapeFactoryBase.h>

#include "PathShapesPlugin.h"
#include "star/StarShapeFactory.h"
#include "rectangle/RectangleShapeFactory.h"
#include "ellipse/EllipseShapeFactory.h"
#include "spiral/SpiralShapeFactory.h"

#include <mutex>

void registerPathShapes()
{
    static std::once_flag once;
    std::call_once(once, [] {
        KoShapeRegistry::instance()->add(new StarShapeFactory());
        KoShapeRegistry::instance()->add(new RectangleShapeFactory());
        KoShapeRegistry::instance()->add(new SpiralShapeFactory());
        KoShapeRegistry::instance()->add(new EllipseShapeFactory());
    });
}

namespace
{
struct PathShapesRegistration
{
    PathShapesRegistration() { registerPathShapes(); }
};

PathShapesRegistration s_pathShapesRegistration;
} // namespace
