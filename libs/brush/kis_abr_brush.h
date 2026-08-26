/*
 *  SPDX-FileCopyrightText: 2010 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2007 Eric Lamarque <eric.lamarque@free.fr>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_ABR_BRUSH_
#define KIS_ABR_BRUSH_

#include <PkImage.h>

#include <kis_scaling_size_brush.h>
#include <kis_types.h>
#include <kis_shared.h>

#include "kritabrush_export.h"

class KisQImagemask;
class KisAbrBrushCollection;
typedef KisSharedPtr<KisQImagemask> KisQImagemaskSP;

class PkString;
class PkStream;


class BRUSH_EXPORT KisAbrBrush : public KisScalingSizeBrush
{

public:

    /// Construct brush to load filename later as brush
    KisAbrBrush(const PkString& filename, KisAbrBrushCollection *parent);
    KisAbrBrush(const KisAbrBrush& rhs);
    KisAbrBrush(const KisAbrBrush& rhs, KisAbrBrushCollection *parent);
    KisAbrBrush &operator=(const KisAbrBrush &rhs) = delete;
    KoResourceSP clone() const override;

    bool isSerializable() const override;
    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override;
    bool saveToDevice(PkStream *dev) const override;

    std::pair<PkString, PkString> resourceType() const override {
        return std::pair<PkString, PkString>(ResourceType::Brushes, ResourceSubType::AbrBrushes);
    }

    /**
     * @return default file extension for saving the brush
     */
    PkString defaultFileExtension() const override;

    PkImage brushTipImage() const override;

    friend class KisAbrBrushCollection;

    void setBrushTipImage(const PkImage& image) override;

    void toXML(PkXmlDocument& d, PkXmlElement& e) const override;

private:
    KisAbrBrushCollection *m_parent;
};

typedef PkSharedPointer<KisAbrBrush> KisAbrBrushSP;

#endif // KIS_ABR_BRUSH_

