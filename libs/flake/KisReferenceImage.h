/*
 * SPDX-FileCopyrightText: 2017 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KISREFERENCEIMAGE_H
#define KISREFERENCEIMAGE_H

#include <functional>

#include <PkSharedDataPointer.h>

#include <KoColor.h>
#include <KoShape.h>
#include <kis_types.h>
#include <kritashapemodel_export.h>
#include <kundo2command.h>

class PkImage;
class PkPointF;
class PkPainter;
class PkRectF;
class KoStore;
class KisCoordinatesConverter;
class KisCanvas2;

/**
 * @brief The KisReferenceImage class represents a single reference image
 */
class KRITASHAPEMODEL_EXPORT KisReferenceImage : public KoShape
{
public:
    using FallbackFileLoader = std::function<PkImage(const PkString &)>;

    struct KRITASHAPEMODEL_EXPORT SetSaturationCommand : public KUndo2Command {
        PkVector<KisReferenceImage*> images;
        PkVector<qreal> oldSaturations;
        qreal newSaturation;

        explicit SetSaturationCommand(const PkList<KoShape *> &images, qreal newSaturation, KUndo2Command *parent = 0);
        void undo() override;
        void redo() override;
    };

    KisReferenceImage();
    KisReferenceImage(const KisReferenceImage &rhs);
    ~KisReferenceImage();

    KoShape *cloneShape() const override;

    static KisReferenceImage * fromQImage(const KisCoordinatesConverter &converter, const PkImage &img);

    /**
     * Load a reference image from specified paint device.
     * @return reference image or null if one could not be loaded
     */
    static KisReferenceImage *
    fromPaintDevice(KisPaintDeviceSP src, const KisCoordinatesConverter &converter);

    void setSaturation(qreal saturation);
    qreal saturation() const;

    void setEmbed(bool embed);
    bool embed();
    bool hasLocalFile();

    void setFilename(const PkString &filename);
    PkString filename() const;
    PkString internalFile() const;

    void paint(PkPainter &gc) const override;

    PkColor getPixel(PkPointF position);

    void saveXml(PkXmlDocument &document, PkXmlElement &parentElement, int id);
    bool saveImage(KoStore *store) const;

    static KisReferenceImage * fromXml(const PkXmlElement &elem);
    bool loadImage(KoStore *store);
    bool loadImage(KoStore *store, const FallbackFileLoader &fallbackLoader);

    PkImage getImage();

private:
    struct Private;
    PkSharedDataPointer<Private> d;
};

#endif // KISREFERENCEIMAGE_H
