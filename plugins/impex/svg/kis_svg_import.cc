/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_svg_import.h"
#include "svg_import_policy.h"

#include <kpluginfactory.h>
#include <QBuffer>

#include <filesystem>
#include <KisDocument.h>
#include <kis_image.h>

#include <SvgParser.h>
#include <KoColorSpaceRegistry.h>
#include "kis_shape_layer.h"
#include <KoShapeControllerBase.h>

K_PLUGIN_FACTORY_WITH_JSON(SVGImportFactory, "krita_svg_import.json", registerPlugin<KisSVGImport>();)

namespace {
QString qtString(const PkString &value)
{
    const std::string utf8 = value.PkToUtf8();
    return QString::fromUtf8(utf8.data(), static_cast<int>(utf8.size()));
}

PkString pkString(const QString &value)
{
    const QByteArray utf8 = value.toUtf8();
    return PkString::PkFromUtf8(utf8.constData(), utf8.size());
}
}

KisSVGImport::KisSVGImport(QObject *parent, const PkVariantList &) : KisImportExportFilter(parent)
{
}

KisSVGImport::~KisSVGImport()
{
}

KisImportExportErrorCode KisSVGImport::convert(KisDocument *document, PkStream *io,  KisPropertiesConfigurationSP configuration)
{
    (void)configuration;

    KisDocument * doc = document;

    std::error_code pathError;
    const std::filesystem::path sourcePath = std::filesystem::u8path(filename().PkToUtf8());
    const std::filesystem::path canonicalPath = std::filesystem::weakly_canonical(sourcePath, pathError);
    const PkString baseXmlDir(
        (pathError ? sourcePath.parent_path() : canonicalPath.parent_path()).u8string().c_str());

    const SvgImportPolicy policy = deterministicSvgImportPolicy();
    const qreal resolution = policy.resolutionPpi;

    // The SVG shape parser is a downstream boundary that still exposes Qt
    // value types. Keep conversion at this one edge; policy and importer state
    // remain toolkit-free and deterministic.
    const PkByteArray sourceBytes = io->readAll();
    QByteArray qtSource(sourceBytes.constData(), sourceBytes.size());
    QBuffer parserStream(&qtSource);
    if (!parserStream.open(QIODevice::ReadOnly)) {
        return ImportExportCodes::ErrorWhileReading;
    }

    auto warnings = QStringList();
    auto errors = QStringList();
    auto fragmentSize = QSizeF();
    auto shapes = KisShapeLayer::createShapesFromSvg(
        &parserStream, qtString(baseXmlDir), QRectF(0, 0, 1200, 800), resolution,
        doc->shapeController()->resourceManager(), false, &fragmentSize, &warnings, &errors);

    if (!warnings.isEmpty()) {
        doc->setWarningMessage(pkString(warnings.join('\n')));
    }
    if (!errors.isEmpty()) {
        doc->setErrorMessage(pkString(errors.join('\n')));
        return ImportExportCodes::FileFormatIncorrect;
    }


    PkRectF rawImageRect(PkPointF(), PkSizeF(fragmentSize.width(), fragmentSize.height()));
    PkRect imageRect(rawImageRect.toAlignedRect());

    const KoColorSpace* cs = KoColorSpaceRegistry::instance()->rgb8();
    KisImageSP image = new KisImage(doc->createUndoStore(), imageRect.width(), imageRect.height(), cs, "svg image");
    image->setResolution(resolution / 72.0, resolution / 72.0);
    doc->setCurrentImage(image);

    KisShapeLayerSP shapeLayer =
            new KisShapeLayer(doc->shapeController(), image,
                              qtString(PkString("Vector Layer")),
                              OPACITY_OPAQUE_U8);

    for (KoShape *shape : shapes) {
        shapeLayer->addShape(shape);
    }

    image->addNode(shapeLayer);
    return ImportExportCodes::OK;
}

#include <kis_svg_import.moc>
