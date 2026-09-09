#include <kis_shared_ptr.h>
/* This file is part of the KDE project
   SPDX-FileCopyrightText: 2011 Thorsten Zachmann <zachmann@kde.org>

   SPDX-License-Identifier: LGPL-2.0-or-later
*/

#include "KoMarkerCollection.h"

#include <PkFileStream.h>

#include "KoMarker.h"
#include <FlakeDebug.h>
#include <KoResourcePaths.h>
#include <SvgParser.h>
#include <KoDocumentResourceManager.h>

#include "kis_debug.h"

// WARNING: there is a bug in GCC! It doesn't warn that we are
//          deleting an uninitialized type here!
#include <KoShape.h>

#include <filesystem>


class KoMarkerCollection::Private
{
public:
    ~Private()
    {
    }

    PkList<KisSharedPtr<KoMarker> > markers;
};

KoMarkerCollection::KoMarkerCollection(PkObject *parent)
: PkObject(parent)
, d(new Private)
{
    // Add no marker so the user can remove a marker from the line.
    d->markers.append(KisSharedPtr<KoMarker>(0));
    // Add default markers
    loadDefaultMarkers();
}

KoMarkerCollection::~KoMarkerCollection()
{
    delete d;
}

void KoMarkerCollection::loadMarkersFromFile(const PkString &svgFile)
{
    PkFileStream file(svgFile);
    if (!std::filesystem::exists(std::filesystem::u8path(file.fileName().PkToUtf8()))) return;

    if (!file.open(PkStream::ReadOnly)) return;

    PkString errorMsg;
    int errorLine = 0;
    int errorColumn;

    PkXmlDocument doc = SvgParser::createDocumentFromSvg(&file, &errorMsg, &errorLine, &errorColumn);
    if (doc.isNull()) {
        errKrita << "Parsing error in " << svgFile << "! Aborting!\n"
        << " In line: " << errorLine << ", column: " << errorColumn << '\n'
        << " Error message: " << errorMsg << '\n';
        errKrita << "Parsing error in the main document at line " << errorLine
                 << ", column " << errorColumn << "\nError message: " << errorMsg;
        return;
    }

    KoDocumentResourceManager manager;
    SvgParser parser(&manager);
    parser.setResolution(PkRectF(0,0,100,100), 72); // initialize with default values
    const std::string parentPath = std::filesystem::u8path(svgFile.PkToUtf8()).parent_path().u8string();
    parser.setXmlBaseDir(PkString::PkFromUtf8(parentPath.data(), static_cast<int>(parentPath.size())));

    parser.setFileFetcher(
        [](const PkString &fileName) {
            PkFileStream file(fileName);
            if (!file.open(PkStream::ReadOnly)) return PkByteArray();

            return file.readAll();
        });

    PkSizeF fragmentSize;
    PkList<KoShape*> shapes = parser.parseSvg(doc.documentElement(), &fragmentSize);
    for (KoShape *shape : shapes) {
        delete shape;
    }

    for (KisSharedPtr<KoMarker> marker : parser.knownMarkers()) {
        addMarker(marker.data());
    }
}

void KoMarkerCollection::loadDefaultMarkers()
{
    PkString filePath = KoResourcePaths::findAsset("markers", "markers.svg");
    loadMarkersFromFile(filePath);
}

PkList<KoMarker*> KoMarkerCollection::markers() const
{
    PkList<KoMarker*> markerList;
    for (KisSharedPtr<KoMarker> m : d->markers) {
        markerList.append(m.data());
    }
    return markerList;
}

KoMarker * KoMarkerCollection::addMarker(KoMarker *marker)
{
    for (KisSharedPtr<KoMarker> m : d->markers) {
        if (marker == m.data()) {
            return marker;
        }
        if (m && *marker == *m) {
            debugFlake << "marker is the same as other";
            return m.data();
        }
    }
    d->markers.append(KisSharedPtr<KoMarker>(marker));
    return marker;
}
