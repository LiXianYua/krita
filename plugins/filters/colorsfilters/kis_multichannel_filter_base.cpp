/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2018 Jouni Pentikainen <joupent@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_multichannel_filter_base.h"

#include <Qt>
#include <QLayout>
#include <QPixmap>
#include <QPainter>
#include <PkXmlDocument.h>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QRegularExpression>

#include "KoChannelInfo.h"
#include "KoBasicHistogramProducers.h"
#include "KoColorModelStandardIds.h"
#include "KoColorSpace.h"
#include "KoColorTransformation.h"
#include "KoCompositeColorTransformation.h"
#include "KoCompositeOp.h"
#include "KoID.h"

#include "kis_signals_blocker.h"

#include "kis_bookmarked_configuration_manager.h"
#include <filter/kis_filter_category_ids.h>
#include <filter/kis_filter_configuration.h>
#include <kis_selection.h>
#include <kis_paint_device.h>
#include <kis_processing_information.h>

#include "kis_histogram.h"
#include "kis_painter.h"

#include "kis_multichannel_utils.h"

KisMultiChannelFilter::KisMultiChannelFilter(const KoID& id, const PkString &entry)
        : KisColorTransformationFilter(id, FiltersCategoryAdjustId, entry)
{
    setSupportsPainting(true);
    setColorSpaceIndependence(TO_LAB16);
}

bool KisMultiChannelFilter::needsTransparentPixels(const KisFilterConfigurationSP config, const KoColorSpace *cs) const
{
    Q_UNUSED(config);
    return cs->colorModelId() == AlphaColorModelID;
}

PkVector<VirtualChannelInfo> KisMultiChannelFilter::getVirtualChannels(const KoColorSpace *cs, int maxChannels)
{
    return KisMultiChannelUtils::getVirtualChannels(cs, maxChannels);
}

int KisMultiChannelFilter::findChannel(const PkVector<VirtualChannelInfo> &virtualChannels,
                                       const VirtualChannelInfo::Type &channelType)
{
    return KisMultiChannelUtils::findChannel(virtualChannels, channelType);
}


KisMultiChannelFilterConfiguration::KisMultiChannelFilterConfiguration(int channelCount, const PkString & name, qint32 version, KisResourcesInterfaceSP resourcesInterface)
        : KisColorTransformationConfiguration(name, version, resourcesInterface)
        , m_channelCount(channelCount)
{
}

KisMultiChannelFilterConfiguration::KisMultiChannelFilterConfiguration(const KisMultiChannelFilterConfiguration &rhs)
    : KisColorTransformationConfiguration(rhs),
      m_channelCount(rhs.m_channelCount),
      m_curves(rhs.m_curves),
      m_transfers(rhs.m_transfers)
{
}

KisMultiChannelFilterConfiguration::~KisMultiChannelFilterConfiguration()
{}

void KisMultiChannelFilterConfiguration::init()
{
    m_curves.clear();
    
    KisColorTransformationConfiguration::setProperty("nTransfers", m_channelCount);

    for (int i = 0; i < m_channelCount; ++i) {
        m_curves.append(getDefaultCurve());

        const PkString name = QLatin1String("curve") + PkString::number(i);
        const PkString value = m_curves.last().toString();
        KisColorTransformationConfiguration::setProperty(name, value);
    }

    updateTransfers();
}

bool KisMultiChannelFilterConfiguration::isCompatible(const KisPaintDeviceSP dev) const
{
    return (int)dev->compositionSourceColorSpace()->channelCount() == m_channelCount;
}

void KisMultiChannelFilterConfiguration::setCurves(PkList<KisCubicCurve> &curves)
{
    // Clean unused properties
    if (curves.size() < m_curves.size()) {
        for (int i = curves.size(); i < m_curves.size(); ++i) {
            const PkString name = QLatin1String("curve") + PkString::number(i);
            KisColorTransformationConfiguration::removeProperty(name);
        }
    }

    m_curves.clear();
    m_curves = curves;
    m_channelCount = curves.size();
    m_activeCurve = qMin(m_activeCurve, m_channelCount - 1);

    updateTransfers();

    // Update properties for python
    KisColorTransformationConfiguration::setProperty("nTransfers", m_channelCount);

    for (int i = 0; i < m_curves.size(); ++i) {
        const PkString name = QLatin1String("curve") + PkString::number(i);
        const PkString value = m_curves[i].toString();
        KisColorTransformationConfiguration::setProperty(name, value);
    }
}

void KisMultiChannelFilterConfiguration::setActiveCurve(int value)
{
    m_activeCurve = value;
    KisColorTransformationConfiguration::setProperty("activeCurve", value);
}

void KisMultiChannelFilterConfiguration::updateTransfer(int index)
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(index >= 0 && index < m_curves.size());
    m_transfers[index] = m_curves[index].uint16Transfer();
}

void KisMultiChannelFilterConfiguration::updateTransfers()
{
    m_transfers.resize(m_channelCount);
    for (int i = 0; i < m_channelCount; i++) {
        m_transfers[i] = m_curves[i].uint16Transfer();
    }
}

const PkVector<PkVector<quint16> >&
KisMultiChannelFilterConfiguration::transfers() const
{
    return m_transfers;
}

const PkList<KisCubicCurve>&
KisMultiChannelFilterConfiguration::curves() const
{
    return m_curves;
}

void KisMultiChannelFilterConfiguration::fromLegacyXML(const PkXmlElement& root)
{
    fromXML(root);
}

void KisMultiChannelFilterConfiguration::fromXML(const PkXmlElement& root)
{
    PkList<KisCubicCurve> curves;
    quint16 numTransfers = 0;
    quint16 numTransfersWithAlpha = 0;
    int activeCurve = -1;
    int version;
    version = root.attribute("version").toInt();

    PkXmlElement e = root.firstChild().toElement();
    PkString attributeName;
    KisCubicCurve curve;
    quint16 index;
    QRegularExpression curveRegexp("curve(\\d+)");
    QRegularExpressionMatch match;

    while (!e.isNull()) {
        if ((attributeName = e.attribute("name")) == "activeCurve") {
            activeCurve = e.text().toInt();
        } else if ((attributeName = e.attribute("name")) == "nTransfers") {
            numTransfers = e.text().toUShort();
        } else if ((attributeName = e.attribute("name")) == "nTransfersWithAlpha") {
            numTransfersWithAlpha = e.text().toUShort();
        } else {
            if (attributeName.contains(curveRegexp, &match)) {

                index = match.captured(1).toUShort();
                index = qMin(index, quint16(curves.count()));

                if (!e.text().isEmpty()) {
                    curve = KisCubicCurve(e.text());
                }
                curves.insert(index, curve);
            }
        }
        e = e.nextSiblingElement();
    }

    /**
     * In Krita 2.9 we stored alpha channel under a separate tag, so we
     * should addend it separately if present
     */
    if (numTransfersWithAlpha > numTransfers) {
        e = root.firstChild().toElement();
        while (!e.isNull()) {
            if ((attributeName = e.attribute("name")) == "alphaCurve") {
                if (!e.text().isEmpty()) {
                    curves.append(KisCubicCurve(e.text()));
                }
            }
            e = e.nextSiblingElement();
        }
    }

    //prepend empty curves for the brightness contrast filter.
    if(getString("legacy") == "brightnesscontrast") {
        if (getString("colorModel") == LABAColorModelID.id()) {
            curves.append(KisCubicCurve());
            curves.append(KisCubicCurve());
            curves.append(KisCubicCurve());
        } else {
            int extraChannels = 5;
            if (getString("colorModel") == CMYKAColorModelID.id()) {
                extraChannels = 6;
            } else if (getString("colorModel") == GrayAColorModelID.id()) {
                extraChannels = 0;
            }
            for(int c = 0; c < extraChannels; c ++) {
                curves.insert(0, KisCubicCurve());
            }
        }
    }
    if (!numTransfers)
        return;

    setVersion(version);
    setCurves(curves);
    setActiveCurve(activeCurve);
}

/**
 * Inherited from KisPropertiesConfiguration
 */
//void KisMultiChannelFilterConfiguration::fromXML(const PkString& s)

void addParamNode(PkXmlDocument& doc,
                  PkXmlElement& root,
                  const PkString &name,
                  const PkString &value)
{
    PkXmlText text = doc.createTextNode(value);
    PkXmlElement t = doc.createElement("param");
    t.setAttribute("name", name);
    t.appendChild(text);
    root.appendChild(t);
}

void KisMultiChannelFilterConfiguration::toXML(PkXmlDocument& doc, PkXmlElement& root) const
{
    /**
     * @code
     * <params version=1>
     *       <param name="nTransfers">3</param>
     *       <param name="curve0">0,0;0.5,0.5;1,1;</param>
     *       <param name="curve1">0,0;1,1;</param>
     *       <param name="curve2">0,0;1,1;</param>
     * </params>
     * @endcode
     */

    root.setAttribute("version", version());

    PkXmlText text;
    PkXmlElement t;

    addParamNode(doc, root, "nTransfers", PkString::number(m_channelCount));

    if (m_activeCurve >= 0) {
        // save active curve if only it has non-default value
        addParamNode(doc, root, "activeCurve", PkString::number(m_activeCurve));
    }

    KisCubicCurve curve;
    PkString paramName;

    for (int i = 0; i < m_curves.size(); ++i) {
        PkString name = QLatin1String("curve") + PkString::number(i);
        PkString value = m_curves[i].toString();

        addParamNode(doc, root, name, value);
    }
}

bool KisMultiChannelFilterConfiguration::compareTo(const KisPropertiesConfiguration *rhs) const
{
    const KisMultiChannelFilterConfiguration *otherConfig = dynamic_cast<const KisMultiChannelFilterConfiguration *>(rhs);

    return otherConfig
        && KisFilterConfiguration::compareTo(rhs)
        && m_channelCount == otherConfig->m_channelCount
        && m_curves == otherConfig->m_curves
        && m_transfers == otherConfig->m_transfers
        && m_activeCurve == otherConfig->m_activeCurve;
}

void KisMultiChannelFilterConfiguration::setProperty(const PkString& name, const PkVariant& value)
{
    if (name == "nTransfers") {
        KIS_SAFE_ASSERT_RECOVER_RETURN(value.canConvert<int>());

        const qint32 newChannelCount = value.toInt();

        if (newChannelCount == m_channelCount) {
            return;
        }

        KisColorTransformationConfiguration::setProperty(name, value);

        m_transfers.resize(newChannelCount);
        if (newChannelCount > m_channelCount) {
            for (qint32 i = m_channelCount; i < newChannelCount; ++i) {
                m_curves.append(getDefaultCurve());
                updateTransfer(i);

                const PkString name = QLatin1String("curve") + PkString::number(i);
                const PkString value = m_curves.last().toString();
                KisColorTransformationConfiguration::setProperty(name, value);
            }
        } else {
            for (qint32 i = newChannelCount; i < m_channelCount; ++i) {
                m_curves.removeLast();

                const PkString name = QLatin1String("curve") + PkString::number(i);
                KisColorTransformationConfiguration::removeProperty(name);
            }
        }

        m_channelCount = newChannelCount;
        invalidateColorTransformationCache();


        return;
    }

    if (name == "activeCurve") {
        setActiveCurve(qBound(0, value.toInt(), m_channelCount));
    }

    int curveIndex;
    if (!curveIndexFromCurvePropertyName(name, curveIndex) ||
        curveIndex < 0 || curveIndex >= m_channelCount) {
        return;
    }

    KIS_SAFE_ASSERT_RECOVER_RETURN(value.canConvert<PkString>());

    m_curves[curveIndex] = KisCubicCurve(value.toString());
    updateTransfer(curveIndex);
    invalidateColorTransformationCache();

    // Query the curve instead of using the value directly, in case of not valid curve string
    KisColorTransformationConfiguration::setProperty(name, m_curves[curveIndex].toString());
}

bool KisMultiChannelFilterConfiguration::curveIndexFromCurvePropertyName(const PkString& name, int& curveIndex) const
{
    QRegularExpression rx("curve(\\d+)");
    QRegularExpressionMatch match;
    if (!name.contains(rx, &match)) {
        return false;
    }

    curveIndex = match.captured(1).toUShort();
    return true;
}

PkList<KisCubicCurve> KisMultiChannelFilterConfiguration::remapLegacyCurves(const KoColorSpace *targetColorSpace,
                                                                          const PkList<KisCubicCurve> &loadedCurves,
                                                                          const PkList<KisCubicCurve> &defaultCurves)
{
    // The configuration does not cover all our channels.
    // This happens when loading a document from an older version, which supported fewer channels.
    // Reset to make sure the unspecified channels have their default values.
    PkList<KisCubicCurve> curves = defaultCurves;

    PkVector<VirtualChannelInfo> virtualChannels = KisMultiChannelFilter::getVirtualChannels(targetColorSpace);

    auto compareChannels =
        [] (const VirtualChannelInfo &lhs, const VirtualChannelInfo &rhs) -> bool {
        return lhs.type() == rhs.type() &&
            (lhs.type() != VirtualChannelInfo::REAL || lhs.pixelIndex() == rhs.pixelIndex());
    };

    /**
     * Adjust the layout of channels in the configuration to the layout of the
     * current version of Krita. When we pass number of loaded channels
     * to getVirtualChannels() it automatically detects the version of Krita
     * the configuration was created in.
     */
    PkVector<VirtualChannelInfo> detectedCurves = KisMultiChannelUtils::getVirtualChannels(targetColorSpace, loadedCurves.size());

    for (auto detectedIt = detectedCurves.begin(); detectedIt != detectedCurves.end(); ++detectedIt) {
        auto dstIt = std::find_if(virtualChannels.begin(), virtualChannels.end(),
                                  [=] (const VirtualChannelInfo &info) {
                                      return compareChannels(*detectedIt, info);
                                  });
        if (dstIt != virtualChannels.end()) {
            const int srcIndex = std::distance(detectedCurves.begin(), detectedIt);
            const int dstIndex = std::distance(virtualChannels.begin(), dstIt);
            curves[dstIndex] = loadedCurves[srcIndex];
        } else {
            warnKrita << "WARNING: failed to find mapping of the channel in the filter configuration:";
            warnKrita << "WARNING:   channel:" << ppVar(detectedIt->name()) << ppVar(detectedIt->type())<< ppVar(detectedIt->pixelIndex());
            warnKrita << "WARNING:";

            for (auto it = detectedCurves.begin(); it != detectedCurves.end(); ++it) {
                warnKrita << "WARNING:   detected channels" << std::distance(detectedCurves.begin(), it) << ":" << it->name();
            }

            for (auto it = virtualChannels.begin(); it != virtualChannels.end(); ++it) {
                warnKrita << "WARNING:   read channels" << std::distance(virtualChannels.begin(), it) << ":" << it->name();
            }
        }
    }

    return curves;
}
