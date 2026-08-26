/*
 *  SPDX-FileCopyrightText: 2010 Boudewijn Rempt <boud@valdyas.org>
 *  SPDX-FileCopyrightText: 2010 Lukáš Tvrdý <lukast.dev@gmail.com>
 *  SPDX-FileCopyrightText: 2007 Eric Lamarque <eric.lamarque@free.fr>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "kis_abr_brush.h"
#include "kis_abr_brush_collection.h"

#include <PkXmlElement.h>
#include <PkFileStream.h>
#include <PkImage.h>
#include <PkPoint.h>
#include <PkMemoryStream.h>
#include <QCryptographicHash>

#include <klocalizedstring.h>

#include <KoColor.h>

#include "kis_datamanager.h"
#include "kis_paint_device.h"
#include "kis_global.h"
#include "kis_image.h"

#define DEFAULT_SPACING 0.25

KisAbrBrush::KisAbrBrush(const PkString& filename, KisAbrBrushCollection *parent)
    : KisScalingSizeBrush(filename)
    , m_parent(parent)
{
    setBrushType(INVALID);
    setSpacing(DEFAULT_SPACING);
}

KisAbrBrush::KisAbrBrush(const KisAbrBrush& rhs)
    : KisScalingSizeBrush(rhs)
    , m_parent(0)
{
    // Warning! The brush became detached from the parent collection!
}

KisAbrBrush::KisAbrBrush(const KisAbrBrush& rhs, KisAbrBrushCollection *parent)
    : KisScalingSizeBrush(rhs)
    , m_parent(parent)
{
}

KoResourceSP KisAbrBrush::clone() const
{
    return KoResourceSP(new KisAbrBrush(*this));
}

bool KisAbrBrush::isSerializable() const
{
    return false;
}

bool KisAbrBrush::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    Q_UNUSED(dev);
    Q_UNUSED(resourcesInterface);
    return false;
}

bool KisAbrBrush::saveToDevice(PkStream *dev) const
{
    Q_UNUSED(dev);
    return false;
}

void KisAbrBrush::setBrushTipImage(const PkImage& image)
{
    setValid(true);
    setBrushType(MASK);

    KisBrush::setBrushTipImage(image);
}

void KisAbrBrush::toXML(PkXmlDocument& d, PkXmlElement& e) const
{
    e.setAttribute("name", name()); // legacy
    predefinedBrushToXML("abr_brush", e);
    KisBrush::toXML(d, e);
}

PkString KisAbrBrush::defaultFileExtension() const
{
    return PkString();
}

PkImage KisAbrBrush::brushTipImage() const
{
    if (KisBrush::brushTipImage().isNull() && m_parent) {
        m_parent->load();
    }
    return KisBrush::brushTipImage();
}
