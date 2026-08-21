/*
 *  SPDX-FileCopyrightText: 2007 Cyrille Berger <cberger@cberger.net>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#ifndef _KO_COLOR_TRANSFORMATION_FACTORY_H_
#define _KO_COLOR_TRANSFORMATION_FACTORY_H_

#include <PkHash.h>
#include <PkStringHash.h>
#include <PkVariant.h>
#include <PkList.h>
#include <PkPair.h>
#include <PkString.h>

class KoColorTransformation;
class KoColorSpace;
class KoID;

#include "kritapigment_export.h"

/**
 * Allow to extend the number of color transformation of a
 * colorspace.
 */
class KRITAPIGMENT_EXPORT KoColorTransformationFactory
{
public:
    explicit KoColorTransformationFactory(const PkString &id);
    virtual ~KoColorTransformationFactory();
public:
    PkString id() const;
public:
    /**
     * @return an empty list if the factory support all type of colorspaces models.
     */
    virtual PkList< PkPair< KoID, KoID > > supportedModels() const = 0;
    virtual KoColorTransformation* createTransformation(const KoColorSpace* colorSpace, PkHash<PkString, PkVariant> parameters) const = 0;
private:
    struct Private;
    Private* const d;
};

#endif
