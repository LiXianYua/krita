/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOCANVASRESOURCESINTERFACE_H
#define KOCANVASRESOURCESINTERFACE_H

#include "kritaresources_export.h"
#include <PkSharedPointer.h>

class PkVariant;

#include <kritaresources_export.h>

/**
 * @brief An abstract class for providing access to canvas resources
 * like current gradient and Fg/Bg colors.
 *
 * Specific implementations may forward the requests either to
 * KoCanvasResourceProvider or to a local storage.
 */
class KRITARESOURCES_EXPORT KoCanvasResourcesInterface
{
public:
    virtual ~KoCanvasResourcesInterface();

    virtual PkVariant resource(int key) const = 0;
};

using KoCanvasResourcesInterfaceSP = PkSharedPointer<KoCanvasResourcesInterface>;

#endif // KOCANVASRESOURCESINTERFACE_H
