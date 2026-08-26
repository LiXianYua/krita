/*
 *  SPDX-FileCopyrightText: 2010 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2007 Eric Lamarque <eric.lamarque@free.fr>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KIS_ABR_BRUSH_COLLECTION_H
#define KIS_ABR_BRUSH_COLLECTION_H

#include <PkImage.h>
#include <PkDataStream.h>
#include <PkString.h>
#include <kis_debug.h>

#include <kis_scaling_size_brush.h>
#include <kis_types.h>
#include <kis_shared.h>
#include <brushengine/kis_paint_information.h>
#include <kis_abr_brush.h>


class PkString;
class PkStream;


struct AbrInfo;

/**
 * load a collection of brushes from an abr file
 */
class BRUSH_EXPORT KisAbrBrushCollection
{

protected:

public:

    /// Construct brush to load filename later as brush
    KisAbrBrushCollection(const PkString& filename);

    ~KisAbrBrushCollection() {}

    bool load();

    bool loadFromDevice(PkStream *dev);

    bool save();

    bool saveToDevice(PkStream* dev) const;

    bool isLoaded() const;

    /**
     * @return a preview of the brush
     */
    PkImage image() const;

    /**
     * @return default file extension for saving the brush
     */
    PkString defaultFileExtension() const;

    PkList<KisAbrBrushSP> brushes() const {
        return m_abrBrushes->values();
    }

    PkSharedPointer<PkMap<PkString, KisAbrBrushSP>> brushesMap() const {
        return m_abrBrushes;
    }

    KisAbrBrushSP brushByName(PkString name) const {
        if (m_abrBrushes->contains(name)) {
            return m_abrBrushes.data()->operator[](name);
        }
        return KisAbrBrushSP();
    }

    PkDateTime lastModified() const {
        return m_lastModified;
    }

    PkString filename() const {
        return m_filename;
    }

protected:
    KisAbrBrushCollection(const KisAbrBrushCollection& rhs);

    void toXML(PkXmlDocument& d, PkXmlElement& e) const;

private:

    qint32 abr_brush_load(PkDataStream & abr, AbrInfo *abr_hdr, const PkString filename, qint32 image_ID, qint32 id);
    qint32 abr_brush_load_v12(PkDataStream & abr, AbrInfo *abr_hdr, const PkString filename, qint32 image_ID, qint32 id);
    quint32 abr_brush_load_v6(PkDataStream & abr, AbrInfo *abr_hdr, const PkString filename, qint32 image_ID, qint32 id);

    bool m_isLoaded;
    PkDateTime m_lastModified;
    PkString m_filename;
    PkSharedPointer<PkMap<PkString, KisAbrBrushSP>> m_abrBrushes;
};

typedef PkSharedPointer<KisAbrBrushCollection> KisAbrBrushCollectionSP;

#endif

