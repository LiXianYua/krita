/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOSHAPEUSERDATA_H
#define KOSHAPEUSERDATA_H

#include <PkObject.h>

#include "kritaflake_export.h"

/**
 * The KoShapeUserData class is used to associate custom data with a shape.
 *
 *  KoShapeUserData provides an abstract interface for container classes
 *  that are used to associate application-specific user data with shapes in KoShape
 *  Generally, subclasses of this class provide functions to allow data to
 *  be stored and retrieved, and instances are attached to KoShape using
 *  KoShape::setUserData(). This makes it possible to store additional data per
 *  shape in a way that allows applications to not know the implementation of a
 *  specific KoShape extending class.
 *
 *  Each subclass should provide a reimplementation of the destructor to ensure that
 *  any private data is automatically cleaned up when user data objects are deleted.
 *
 * Runtime type checks use standard C++ RTTI across the exported flake ABI.
 */
class KRITAFLAKE_EXPORT KoShapeUserData : public PkObject
{
public:
    /// Constructor
    explicit KoShapeUserData(PkObject *parent = nullptr);
    ~KoShapeUserData() override;

    virtual KoShapeUserData* clone() const = 0;

protected:
    KoShapeUserData(const KoShapeUserData &rhs);
};

#endif
