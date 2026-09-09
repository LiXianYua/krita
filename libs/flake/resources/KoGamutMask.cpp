/*
 *  SPDX-FileCopyrightText: 2018 Anna Medonosova <anna.medonosova@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KoGamutMask.h"
#include <KisResourceThumbnailCodec.h>

#include <cstring>

#include <PkVector.h>
#include <PkContainerAlgo.h>
#include <PkString.h>
#include <PkFileStream.h>
#include <PkList.h>
#include <PkXmlDocument.h>
#include <PkXmlElement.h>
#include <PkByteArray.h>
#include <PkMemoryStream.h>
#include <PkScopedPointer.h>

#include <FlakeDebug.h>

#include <KoStore.h>
#include <KoStoreDevice.h>
#include <KoDocumentResourceManager.h>
#include <PkImage.h>
#include <SvgParser.h>
#include <SvgWriter.h>
#include <KoShape.h>
#include <kis_assert.h>
#include <PkTransform.h>
#include <KoMarker.h>

//#include <kis_debug.h>

KoGamutMaskShape::KoGamutMaskShape(KoShape* shape)
    : m_maskShape(shape)
{
}

KoGamutMaskShape::KoGamutMaskShape()
{
};

KoGamutMaskShape::~KoGamutMaskShape()
{
    delete m_maskShape;
};

KoShape* KoGamutMaskShape::koShape()
{
    return m_maskShape;
}

bool KoGamutMaskShape::coordIsClear(const PkPointF& coord) const
{
    bool isClear = m_maskShape->hitTest(coord);

    return isClear;
}

void KoGamutMaskShape::paint(PkPainter &painter)
{
    painter.save();
    painter.setTransform(m_maskShape->absoluteTransformation(), true);
    m_maskShape->paint(painter);
    painter.restore();
}

void KoGamutMaskShape::paintStroke(PkPainter &painter)
{
    painter.save();
    painter.setTransform(m_maskShape->absoluteTransformation(), true);
    m_maskShape->paintStroke(painter);
    painter.restore();
}

struct KoGamutMask::Private {
    PkString name;
    PkString title;
    PkByteArray data;
    PkVector<KoGamutMaskShape*> maskShapes;
    PkVector<KoGamutMaskShape*> previewShapes;
    PkSizeF maskSize;
    int rotation {0};
};

KoGamutMask::KoGamutMask(const PkString &filename)
    : KoResource(filename)
    , d(new Private)
{
    d->maskSize = PkSizeF(144.0,144.0);
    setRotation(0);
}

KoGamutMask::KoGamutMask()
    : KoResource(PkString())
    , d(new Private)
{
    d->maskSize = PkSizeF(144.0,144.0);
    setRotation(0);
}

KoGamutMask::KoGamutMask(KoGamutMask* rhs)
    : KoGamutMask(*rhs)
{
}

KoGamutMask::KoGamutMask(const KoGamutMask &rhs)
    : KoResource(rhs)
    , d(new Private)
{
    setTitle(rhs.title());
    setDescription(rhs.description());
    d->maskSize = rhs.d->maskSize;

    PkList<KoShape*> newShapes;
    for(KoShape* sh: rhs.koShapes()) {
        newShapes.append(sh->cloneShape());
    }
    setMaskShapes(newShapes);
}

KoResourceSP KoGamutMask::clone() const
{
    return KoResourceSP(new KoGamutMask(*this));
}

KoGamutMask::~KoGamutMask()
{
    pkDeleteAll(d->maskShapes);
    pkDeleteAll(d->previewShapes);
    delete d;
}

bool KoGamutMask::coordIsClear(const PkPointF& coord, bool preview)
{
    PkVector<KoGamutMaskShape*>* shapeVector;

    if (preview && !d->previewShapes.isEmpty()) {
        shapeVector = &d->previewShapes;
    } else {
        shapeVector = &d->maskShapes;
    }

    for(KoGamutMaskShape* shape: *shapeVector) {
        if (shape->coordIsClear(coord) == true) {
            return true;
        }
    }

    return false;
}

void KoGamutMask::paint(PkPainter &painter, bool preview)
{
    PkVector<KoGamutMaskShape*>* shapeVector;

    if (preview && !d->previewShapes.isEmpty()) {
        shapeVector = &d->previewShapes;
    } else {
        shapeVector = &d->maskShapes;
    }

    for(KoGamutMaskShape* shape: *shapeVector) {
        shape->paint(painter);
    }
}

void KoGamutMask::paintStroke(PkPainter &painter, bool preview)
{
    PkVector<KoGamutMaskShape*>* shapeVector;

    if (preview && !d->previewShapes.isEmpty()) {
        shapeVector = &d->previewShapes;
    } else {
        shapeVector = &d->maskShapes;
    }

    for(KoGamutMaskShape* shape: *shapeVector) {
        shape->paintStroke(painter);
    }
}

PkTransform KoGamutMask::maskToViewTransform(qreal viewSize)
{
    // apply mask rotation before drawing
    PkPointF centerPoint(viewSize*0.5, viewSize*0.5);

    PkTransform transform;
    transform.translate(centerPoint.x(), centerPoint.y());
    transform.rotate(rotation());
    transform.translate(-centerPoint.x(), -centerPoint.y());

    qreal scale = viewSize/(maskSize().width());
    transform.scale(scale, scale);

    return transform;
}

PkTransform KoGamutMask::viewToMaskTransform(qreal viewSize)
{
    PkPointF centerPoint(viewSize*0.5, viewSize*0.5);

    PkTransform transform;
    qreal scale = viewSize/(maskSize().width());
    transform.scale(1/scale, 1/scale);

    transform.translate(centerPoint.x(), centerPoint.y());
    transform.rotate(-rotation());
    transform.translate(-centerPoint.x(), -centerPoint.y());

    return transform;
}

bool KoGamutMask::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    (void)resourcesInterface;

    if (!dev->isOpen()) dev->open(PkStream::ReadOnly);

    d->data = dev->readAll();

    // TODO: test
    KIS_ASSERT_RECOVER_RETURN_VALUE(d->data.size() != 0, false);

    if (filename().isEmpty()) {
        warnFlake << "Cannot load gamut mask" << name() << "there is no filename set";
        return false;
    }

    if (d->data.isEmpty()) {
        PkFileStream file(filename());
        if (file.size() == 0) {
            warnFlake << "Cannot load gamut mask" << name() << "there is no data available";
            return false;
        }

        if (!file.open(PkStream::ReadOnly)) {
            warnFlake << "Cannot load gamut mask" << name() << ":" << file.errorString();
            return false;
        }
        d->data = file.readAll();
        file.close();
    }

    PkMemoryStream buf;
    buf.open(PkStream::WriteOnly);
    buf.write(d->data.constData(), static_cast<PkStream::pk_int64>(d->data.size()));
    buf.close();
    buf.open(PkStream::ReadOnly);

    PkScopedPointer<KoStore> store(KoStore::createStore(&buf, KoStore::Read, PkByteArray("application/x-krita-gamutmask"), KoStore::Zip));
    if (!store || store->bad()) return false;

    bool storeOpened = store->open("gamutmask.svg");
    if (!storeOpened) { return false; }

    const PkByteArray ba = store->read(store->size());
    store->close();

    if (ba.size() == 0) { // empty gamutmask.svg is possible when the first temporary resource is saved
        setMaskShapes(PkList<KoShape*>());
        d->maskSize = PkSizeF(0, 0);
        d->title = "";
    } else {

        PkString errorMsg;
        int errorLine = 0;
        int errorColumn = 0;

        PkXmlDocument xmlDocument = SvgParser::createDocumentFromSvg(ba, &errorMsg, &errorLine, &errorColumn);
        if (xmlDocument.isNull()) {

            errorFlake << "Parsing error in " << filename() << "! Aborting!"
                       << " In line: " << errorLine << ", column: " << errorColumn
                       << " Error message: " << errorMsg;
            errorFlake << "Parsing error in the main document at line" << errorLine
                       << ", column" << errorColumn
                       << "Error message: " << errorMsg;

            return false;
        }

        KoDocumentResourceManager manager;
        SvgParser parser(&manager);
        parser.setResolution(PkRectF(0,0,100,100), 72); // initialize with default values
        PkSizeF fragmentSize;

        PkList<KoShape*> shapes = parser.parseSvg(xmlDocument.documentElement(), &fragmentSize);

        d->maskSize = fragmentSize;

        d->title = parser.documentTitle();
        setName(d->title);
        setDescription(parser.documentDescription());

        setMaskShapes(shapes);

    }



    if (store->open("preview.png")) {
        const PkByteArray pngData = store->read(store->size());
        PkImage preview = KisResourceThumbnailCodec::decodePng(pngData);
        setImage(preview);

        (void)store->close();
    }

    buf.close();

    setValid(true);

    return true;
}

void KoGamutMask::setMaskShapes(PkList<KoShape*> shapes)
{
    setMaskShapesToVector(shapes, d->maskShapes);
}

PkList<KoShape*> KoGamutMask::koShapes() const
{
    PkList<KoShape*> shapes;
    for(KoGamutMaskShape* maskShape: d->maskShapes) {
        shapes.append(maskShape->koShape());
    }

    return shapes;
}

bool KoGamutMask::saveToDevice(PkStream *dev) const
{
    KoStore* store(KoStore::createStore(dev, KoStore::Write, PkByteArray("application/x-krita-gamutmask"), KoStore::Zip));
    if (!store || store->bad()) return false;

    PkList<KoShape*> shapes = koShapes();

    std::sort(shapes.begin(), shapes.end(), KoShape::compareShapeZIndex);

    if (!store->open("gamutmask.svg")) {
        return false;
    }

    KoStoreDevice storeDev(store);
    storeDev.open(PkStream::WriteOnly);

    SvgWriter writer(shapes);
    writer.setDocumentTitle(d->title);
    writer.setDocumentDescription(description());

    writer.save(storeDev, d->maskSize);

    if (!store->close()) { return false; }


    if (!store->open("preview.png")) {
        return false;
    }

    const PkByteArray pngBytes = KisResourceThumbnailCodec::encodePng(image());

    KoStoreDevice previewDev(store);
    previewDev.open(PkStream::WriteOnly);
    previewDev.write(pngBytes.constData(), static_cast<PkStream::pk_int64>(pngBytes.size()));
    if (!store->close()) { return false; }

    return store->finalize();
}

PkString KoGamutMask::title() const
{
    return d->title;
}

void KoGamutMask::setTitle(PkString title)
{
    d->title = title;
    setName(title);
}

PkString KoGamutMask::description() const
{
    PkMap<PkString, PkVariant> m = metadata();
    return m.value(PkString("description")).toString();
}

void KoGamutMask::setDescription(PkString description)
{
    addMetaData(PkString("description"), PkVariant(description));
}

PkString KoGamutMask::defaultFileExtension() const
{
    return PkString(".kgm");
}

int KoGamutMask::rotation()
{
    return d->rotation;
}

void KoGamutMask::setRotation(int rotation)
{
    d->rotation = rotation;
}

PkSizeF KoGamutMask::maskSize()
{
    return d->maskSize;
}

void KoGamutMask::setPreviewMaskShapes(PkList<KoShape*> shapes)
{
    setMaskShapesToVector(shapes, d->previewShapes);
}

void KoGamutMask::setMaskShapesToVector(PkList<KoShape *> shapes, PkVector<KoGamutMaskShape *> &targetVector)
{
    targetVector.clear();

    for(KoShape* sh: shapes) {
        KoGamutMaskShape* maskShape = new KoGamutMaskShape(sh);
        targetVector.append(maskShape);
    }
}

// clean up when ending mask preview
void KoGamutMask::clearPreview()
{
    d->previewShapes.clear();
}
