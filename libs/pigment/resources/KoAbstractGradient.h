/*
    SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef KOABSTRACTGRADIENT_H
#define KOABSTRACTGRADIENT_H

#include <PkGradient.h>
#include <PkImage.h>
#include <PkSharedPointer.h>
#include <PkString.h>
#include <PkDebug.h>

#include "KoColorSpace.h"
#include <KoResource.h>
#include <kritapigment_export.h>

class KoAbstractGradient;
typedef PkSharedPointer<KoAbstractGradient> KoAbstractGradientSP;

class KoCanvasResourcesInterface;
using KoCanvasResourcesInterfaceSP = PkSharedPointer<KoCanvasResourcesInterface>;

class KoColor;

/**
 * KoAbstractGradient is the base class of all gradient resources
 */
class KRITAPIGMENT_EXPORT KoAbstractGradient : public KoResource
{
public:
    explicit KoAbstractGradient(const PkString &filename);
    ~KoAbstractGradient() override;

    /**
    * Creates a gradient from the gradient.
    * The resulting gradient might differ from original gradient
    */
    virtual PkGradient* toQGradient() const {
        return new PkGradient();
    }

    /// gets the color at position 0 <= t <= 1
    virtual void colorAt(KoColor&, qreal t) const;

    void setColorSpace(KoColorSpace* colorSpace);
    const KoColorSpace * colorSpace() const;

    void setSpread(PkGradientEnums::Spread spreadMethod);
    PkGradientEnums::Spread spread() const;

    void setType(PkGradientEnums::Type repeatType);
    PkGradientEnums::Type type() const;

    void updatePreview();

    PkImage generatePreview(int width, int height) const;
    PkImage generatePreview(int width, int height, KoCanvasResourcesInterfaceSP canvasResourcesInterface) const;

    KoAbstractGradient(const KoAbstractGradient &rhs);

    KoAbstractGradientSP cloneAndBakeVariableColors(KoCanvasResourcesInterfaceSP canvasResourcesInterface) const;
    virtual void bakeVariableColors(KoCanvasResourcesInterfaceSP canvasResourcesInterface);

    KoAbstractGradientSP cloneAndUpdateVariableColors(KoCanvasResourcesInterfaceSP canvasResourcesInterface) const;
    virtual void updateVariableColors(KoCanvasResourcesInterfaceSP canvasResourcesInterface);

private:
    struct Private;
    Private* const d;
};

inline PkDebug operator<<(PkDebug dbg, const KoAbstractGradientSP res)
{
    if (!res) {
        dbg.noquote() << "NULL Gradient";
    }
    else {
        dbg.nospace() << "[Gradient] Name: " << res->name()
                      << " Version: " << res->version()
                      << " Filename: " << res->filename()
                      << " MD5: " << res->md5Sum(false)
                      << " Type: " << res->resourceType()
                      << " Valid: " << res->valid()
                      << " Storage: " << res->storageLocation();
    }
    return dbg.space();
}


#endif // KOABSTRACTGRADIENT_H
