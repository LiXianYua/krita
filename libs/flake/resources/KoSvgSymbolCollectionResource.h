/*  This file is part of the KDE project
    SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>

    SPDX-License-Identifier: LGPL-2.1-or-later

 */
#ifndef KOSVGSYMBOLCOLLECTIONRESOURCE
#define KOSVGSYMBOLCOLLECTIONRESOURCE

#include <QObject>
#include <PkColor.h>
#include <PkVector.h>
#include <PkScopedPointer.h>
#include <PkImage.h>
#include <PkPainter.h>

#include <KoResource.h>
#include <KisResourceTypes.h>
#include <PkString.h>

#include <KoShape.h>
#include <KoShapeGroup.h>
#include <KoShapeManager.h>


#include "kritaflake_export.h"

struct KRITAFLAKE_EXPORT KoSvgSymbol {
    KoSvgSymbol() {}
    KoSvgSymbol(const PkString &_title)
        : title(_title) {}

    KoSvgSymbol(const KoSvgSymbol &rhs)
        : id(rhs.id),
          title(rhs.title),
          shape(rhs.shape->cloneShape())
    {
    }

    ~KoSvgSymbol()
    {
        delete shape;
    }

    PkString id;
    PkString title;
    KoShape *shape {0};
    PkImage icon(int size);

    bool operator==(const KoSvgSymbol& rhs) const {
        return title == rhs.title;
    }
};

/**
 * Loads an svg file that contains "symbol" objects and creates a collection of those objects.
 */
class KRITAFLAKE_EXPORT KoSvgSymbolCollectionResource : public KoResource
{
public:

    /**
     */
    explicit KoSvgSymbolCollectionResource(const PkString &filename);

    /// Create an empty color set
    KoSvgSymbolCollectionResource();
    ~KoSvgSymbolCollectionResource() override;

    KoSvgSymbolCollectionResource(const KoSvgSymbolCollectionResource &rhs);
    KoSvgSymbolCollectionResource &operator=(const KoSvgSymbolCollectionResource &rhs) = delete;
    KoResourceSP clone() const override;

    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override;
    bool saveToDevice(PkStream* dev) const override;

    PkString defaultFileExtension() const override;

    std::pair<PkString, PkString> resourceType() const override
    {
        return std::pair<PkString, PkString>(ResourceType::Symbols, PkString());
    }

    PkString title() const;
    PkString description() const;
    PkString creator() const;
    PkString rights() const;
    PkString language() const;
    PkStringList subjects() const;
    PkString license() const;
    PkStringList permits() const;

    PkVector<KoSvgSymbol *> symbols() const;


private:

    struct Private;
    const PkScopedPointer<Private> d;

};
#endif // KOSVGSYMBOLCOLLECTIONRESOURCE
