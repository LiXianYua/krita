/*
 *  SPDX-FileCopyrightText: 1999 Matthias Elter <me@kde.org>
 *  SPDX-FileCopyrightText: 2002 Patrick Julien <freak@codepimps.org>
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_GBR_BRUSH_
#define KIS_GBR_BRUSH_

#include <PkImage.h>

#include "KisColorfulBrush.h"
#include <kis_types.h>
#include <kis_shared.h>
#include <brushengine/kis_paint_information.h>

#include "kritabrush_export.h"

class KisQImagemask;
typedef KisSharedPtr<KisQImagemask> KisQImagemaskSP;

class PkString;
class PkStream;

class BRUSH_EXPORT KisGbrBrush : public KisColorfulBrush
{

protected:

public:

    /// Construct brush to load filename later as brush
    KisGbrBrush(const PkString& filename);

    /// Load brush from the specified data, at position dataPos, and set the filename
    KisGbrBrush(const PkString& filename,
                const PkByteArray & data,
                qint32 & dataPos);

    /// Load brush from the specified paint device, in the specified region
    KisGbrBrush(KisPaintDeviceSP image, int x, int y, int w, int h);

    /// Load brush as a copy from the specified PkImage (handy when you need to copy a brush!)
    KisGbrBrush(const PkImage& image, const PkString& name = PkString());

    ~KisGbrBrush() override;

    KisGbrBrush(const KisGbrBrush& rhs);

    KoResourceSP clone() const override;

    KisGbrBrush &operator=(const KisGbrBrush &rhs);

    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override;
    bool saveToDevice(PkStream* dev) const override;

    std::pair<PkString, PkString> resourceType() const override {
        return std::pair<PkString, PkString>(ResourceType::Brushes, ResourceSubType::GbrBrushes);
    }

    /**
     * Convert the mask to inverted gray scale, so it is alpha mask.
     * It can be used as MASK brush type. This operates on the data of the brush,
     * so it destruct the original brush data.
     *
     * @param preserveAlpha convert to grayscale, but save as full RGBA format, to allow
     *                      preserving lightness option
     */
    virtual void makeMaskImage(bool preserveAlpha);

    /**
     * @return default file extension for saving the brush
     */
    PkString defaultFileExtension() const override;

protected:
    /**
     * save the content of this brush to an IO device
     */
    friend class KisImageBrushesPipe;
    friend class KisBrushExport;

    void setBrushTipImage(const PkImage& image) override;

    void toXML(PkXmlDocument& d, PkXmlElement& e) const override;

private:

    bool init();
    bool initFromPaintDev(KisPaintDeviceSP image, int x, int y, int w, int h);

    struct Private;
    Private* const d;
};

typedef PkSharedPointer<KisGbrBrush> KisGbrBrushSP;

#endif // KIS_GBR_BRUSH_

