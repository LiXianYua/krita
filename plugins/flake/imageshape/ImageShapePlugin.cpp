/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2009 Inge Wallin <inge@lysator.liu.se>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "ImageShapePlugin.h"

#include <KoShapeRegistry.h>

#include "ImageShapeFactory.h"

#include <mutex>

void registerImageShape()
{
    static std::once_flag once;
    std::call_once(once, [] {
        KoShapeRegistry::instance()->add(new ImageShapeFactory());
    });
}

namespace
{
struct ImageShapeRegistration
{
    ImageShapeRegistration() { registerImageShape(); }
};

ImageShapeRegistration s_imageShapeRegistration;
} // namespace
