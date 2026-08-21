/*
    SPDX-FileCopyrightText: 2005 Tim Beaulen <tbscope@gmail.org>
    SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
    SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>
    SPDX-FileCopyrightText: 2021 L. E. Segovia <amy@amyspark.me>

    SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include <PkXmlCompat.h>

#include <resources/KoStopGradient.h>

#include <array>
#include <cfloat>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <vector>

#include <PkTextStream.h>
#include <PkStringList.h>
#include <PkVariant.h>

#include "KoColorSpaceRegistry.h"
#include <KoColorSpaceEngine.h>
#include <KoColorProfile.h>
#include "KoMixColorsOp.h"

#include "kis_dom_utils.h"

#include <KoColorModelStandardIds.h>
#include <KoXmlNS.h>

#include <KoCanvasResourcesIds.h>
#include <KoCanvasResourcesInterface.h>

#include <KisMpl.h>

namespace {

// 十六进制（小写/大写）→ 字节（对齐 Qt5 的 fromHex）。
PkByteArray pkFromHex(const PkString &hex)
{
    const std::string in = hex.PkToUtf8();
    std::vector<std::uint8_t> out;
    out.reserve(in.size() / 2);
    const auto hexVal = [](char c) -> int {
        if (c >= '0' && c <= '9') return c - '0';
        if (c >= 'a' && c <= 'f') return c - 'a' + 10;
        if (c >= 'A' && c <= 'F') return c - 'A' + 10;
        return 0;
    };
    for (std::size_t i = 0; i + 1 < in.size(); i += 2) {
        out.push_back(static_cast<std::uint8_t>((hexVal(in[i]) << 4) | hexVal(in[i + 1])));
    }
    return PkByteArray(out);
}

// 字节 → 小写十六进制（对齐 Qt5 的 toHex）。
PkString pkToHex(const PkByteArray &ba)
{
    static const char digits[] = "0123456789abcdef";
    std::string out;
    out.reserve(static_cast<std::size_t>(ba.size()) * 2);
    const char *data = ba.constData();
    for (int i = 0; i < ba.size(); ++i) {
        const unsigned char b = static_cast<unsigned char>(data[i]);
        out.push_back(digits[b >> 4]);
        out.push_back(digits[b & 0xF]);
    }
    return PkString::PkFromUtf8(out.data(), static_cast<int>(out.size()));
}

// 空白折叠为单空格并 trim（对齐 Qt5 的 simplified）。
PkString pkSimplified(const PkString &s)
{
    const std::string in = s.PkToUtf8();
    std::string out;
    out.reserve(in.size());
    bool lastWasSpace = false;
    bool leading = true;
    for (const char c : in) {
        const bool isWs = (c == ' ' || c == '\t' || c == '\n' || c == '\r' || c == '\f' || c == '\v');
        if (isWs) {
            if (lastWasSpace || leading) {
                continue;
            }
            lastWasSpace = true;
            out.push_back(' ');
        } else {
            lastWasSpace = false;
            leading = false;
            out.push_back(c);
        }
    }
    return PkString::PkFromUtf8(out.data(), static_cast<int>(out.size()));
}

// 移除全部该 ASCII 字符（对齐 Qt5 的 remove(ch)）。
PkString pkRemoveAll(const PkString &s, char ch)
{
    const std::string in = s.PkToUtf8();
    std::string out;
    out.reserve(in.size());
    for (const char c : in) {
        if (c != ch) {
            out.push_back(c);
        }
    }
    return PkString::PkFromUtf8(out.data(), static_cast<int>(out.size()));
}

// 按分隔符切分并丢弃空段（对齐 Qt5 的 split(sep, SkipEmptyParts)）。
PkStringList pkSplitSkipEmpty(const PkString &s, char16_t sep)
{
    PkStringList result;
    for (const PkString &part : s.split(sep)) {
        if (!part.isEmpty()) {
            result.push_back(part);
        }
    }
    return result;
}

} // namespace


KoStopGradient::KoStopGradient(const PkString& filename)
    : KoAbstractGradient(filename)
{
}

KoStopGradient::~KoStopGradient()
{
}

KoStopGradient::KoStopGradient(const KoStopGradient &rhs)
    : KoAbstractGradient(rhs)
    , m_stops(rhs.m_stops)
    , m_start(rhs.m_start)
    , m_stop(rhs.m_stop)
    , m_focalPoint(rhs.m_focalPoint)
{
}

bool KoStopGradient::operator==(const KoStopGradient& rhs) const
{
    return
        *colorSpace() == *rhs.colorSpace() &&
        spread() == rhs.spread() &&
        type() == rhs.type() &&
        m_start == rhs.m_start &&
        m_stop == rhs.m_stop &&
        m_focalPoint == rhs.m_focalPoint &&
            m_stops == rhs.m_stops;
}

KoResourceSP KoStopGradient::clone() const
{
    return KoResourceSP(new KoStopGradient(*this));
}

bool KoStopGradient::loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface)
{
    Q_UNUSED(resourcesInterface);
    loadSvgGradient(dev);
    if (m_stops.count() >= 2) {
        setValid(true);
    }
    updatePreview();
    return true;
}

PkGradient* KoStopGradient::toQGradient() const
{
    PkGradient* gradient;

    switch (type()) {
    case PkGradientEnums::LinearGradient: {
        gradient = new PkGradient(PkGradientEnums::LinearGradient);
        gradient->setStart(m_start);
        gradient->setFinalStop(m_stop);
        break;
    }
    case PkGradientEnums::RadialGradient: {
        PkPointF diff = m_stop - m_start;
        qreal radius = sqrt(diff.x() * diff.x() + diff.y() * diff.y());
        gradient = new PkGradient(PkGradientEnums::RadialGradient);
        gradient->setCenter(m_start);
        gradient->setRadius(radius);
        gradient->setFocalPoint(m_focalPoint);
        break;
    }
    case PkGradientEnums::ConicalGradient: {
        qreal angle = atan2(m_start.y(), m_start.x()) * 180.0 / M_PI;
        if (angle < 0.0)
            angle += 360.0;
        gradient = new PkGradient(PkGradientEnums::ConicalGradient);
        gradient->setCenter(m_start);
        gradient->setAngle(angle);
        break;
    }
    default:
        return 0;
    }
    PkColor color;
    for (PkList<KoGradientStop>::const_iterator i = m_stops.begin(); i != m_stops.end(); ++i) {
        i->color.toQColor(&color);
        gradient->setColorAt(i->position, color);
    }

    gradient->setCoordinateMode(PkGradientEnums::ObjectBoundingMode);
    gradient->setSpread(this->spread());

    return gradient;
}

bool KoStopGradient::stopsAt(KoGradientStop& leftStop, KoGradientStop& rightStop, qreal t) const
{
    if (!m_stops.count())
        return false;

    KIS_SAFE_ASSERT_RECOVER(!qIsNaN(t)) { // if it's nan, it would crash in the last 'else'
        leftStop = m_stops.first();
        rightStop = KoGradientStop(-std::numeric_limits<double>::infinity(), leftStop.color, leftStop.type);
        return true;
    }

    if (t <= m_stops.first().position || m_stops.count() == 1) {
        // we have only one stop or t is before the first stop
        leftStop = m_stops.first();
        rightStop = KoGradientStop(-std::numeric_limits<double>::infinity(), leftStop.color, leftStop.type);
        return true;
    } else if (t >= m_stops.last().position) {
        // t is after the last stop
        rightStop = m_stops.last();
        leftStop = KoGradientStop(std::numeric_limits<double>::infinity(), rightStop.color, rightStop.type);
        return true;
    } else {
        // we have at least two color stops
        // -> find the two stops which frame our t
        auto it = std::lower_bound(m_stops.begin(), m_stops.end(), KoGradientStop(t, KoColor(), COLORSTOP),
                                   kismpl::mem_less(&KoGradientStop::position));
        leftStop = *(it - 1);
        rightStop = *(it);
        return true;
    }
}

void KoStopGradient::colorAt(KoColor& dst, qreal t) const
{
    KoGradientStop leftStop, rightStop;
    if (!stopsAt(leftStop, rightStop, t)) return;

    const KoColorSpace *mixSpace = dst.colorSpace();

    KoColor buffer(mixSpace);
    KoColor startDummy(leftStop.color, mixSpace);
    KoColor endDummy(rightStop.color, mixSpace);

    const std::array<quint8 *, 2> colors = {{startDummy.data(), endDummy.data()}};

    qreal localT = NAN;
    qreal stopDistance = rightStop.position - leftStop.position;
    if (stopDistance < DBL_EPSILON) {
        localT = 0.5;
    } else {
        localT = (t - leftStop.position) / stopDistance;
    }
    std::array<qint16, 2> colorWeights {};
    colorWeights[0] = std::lround((1.0 - localT) * qint16_MAX);
    colorWeights[1] = qint16_MAX - colorWeights[0];

    mixSpace->mixColorsOp()->mixColors(colors.data(), colorWeights.data(), 2, buffer.data(), qint16_MAX);

    dst = buffer;
}

PkSharedPointer<KoStopGradient> KoStopGradient::fromQGradient(const PkGradient *gradient)
{
    if (!gradient)
        return PkSharedPointer<KoStopGradient>(0);

    PkSharedPointer<KoStopGradient> newGradient(new KoStopGradient(PkString()));
    newGradient->setType(gradient->type());
    newGradient->setSpread(gradient->spread());

    switch (gradient->type()) {
    case PkGradientEnums::LinearGradient: {
        newGradient->m_start = gradient->start();
        newGradient->m_stop = gradient->finalStop();
        newGradient->m_focalPoint = gradient->start();
        break;
    }
    case PkGradientEnums::RadialGradient: {
        newGradient->m_start = gradient->center();
        newGradient->m_stop = gradient->center() + PkPointF(gradient->radius(), 0);
        newGradient->m_focalPoint = gradient->focalPoint();
        break;
    }
    case PkGradientEnums::ConicalGradient: {
        qreal radian = gradient->angle() * M_PI / 180.0;
        newGradient->m_start = gradient->center();
        newGradient->m_stop = PkPointF(100.0 * cos(radian), 100.0 * sin(radian));
        newGradient->m_focalPoint = gradient->center();
        break;
    }
    default:
        return PkSharedPointer<KoStopGradient>(0);
    }

    for (const PkGradientStop & stop : gradient->stops()) {
        KoColor color(newGradient->colorSpace());
        color.fromQColor(stop.color);
        newGradient->m_stops.append(KoGradientStop(stop.offset, color, COLORSTOP));
    }

    newGradient->setValid(true);

    return newGradient;
}

void KoStopGradient::setStops(PkList< KoGradientStop > stops)
{
    m_stops.clear();
    m_hasVariableStops = false;
    KoColor color;
    for (const KoGradientStop & stop : stops) {
        color = stop.color;
        m_stops.append(KoGradientStop(stop.position, color, stop.type));
        if (stop.type != COLORSTOP) {
            m_hasVariableStops = true;
        }
    }
    if (m_stops.count() >= 2) {
        setValid(true);
    } else {
        setValid(false);
    }
    updatePreview();
}

PkList<KoGradientStop> KoStopGradient::stops() const
{
    return m_stops;
}

PkVector<int> KoStopGradient::requiredCanvasResources() const
{
    PkVector<int> result;

    if (std::find_if_not(m_stops.begin(), m_stops.end(),
                         kismpl::mem_equal_to(&KoGradientStop::type, COLORSTOP))
        != m_stops.end()) {

        result << KoCanvasResource::ForegroundColor << KoCanvasResource::BackgroundColor;
    }

    return result;
}

void KoStopGradient::bakeVariableColors(KoCanvasResourcesInterfaceSP canvasResourcesInterface)
{
    const KoColor fgColor = canvasResourcesInterface->resource(KoCanvasResource::ForegroundColor).value<KoColor>();
    const KoColor bgColor = canvasResourcesInterface->resource(KoCanvasResource::BackgroundColor).value<KoColor>();

    for (auto it = m_stops.begin(); it != m_stops.end(); ++it) {
        if (it->type == FOREGROUNDSTOP) {
            it->color = fgColor;
            it->type = COLORSTOP;
        } else if (it->type == BACKGROUNDSTOP) {
            it->color = bgColor;
            it->type = COLORSTOP;
        }
    }
}

void KoStopGradient::updateVariableColors(KoCanvasResourcesInterfaceSP canvasResourcesInterface)
{
    const KoColor fgColor = canvasResourcesInterface->resource(KoCanvasResource::ForegroundColor).value<KoColor>();
    const KoColor bgColor = canvasResourcesInterface->resource(KoCanvasResource::BackgroundColor).value<KoColor>();

    for (auto it = m_stops.begin(); it != m_stops.end(); ++it) {
        if (it->type == FOREGROUNDSTOP) {
            it->color = fgColor;
        } else if (it->type == BACKGROUNDSTOP) {
            it->color = bgColor;
        }
    }
}

void KoStopGradient::loadSvgGradient(PkStream* file)
{
    PkXmlDocument doc;

    if (!(doc.setContent(file))) {
        file->close();
    } else {
        PkHash<PkString, const KoColorProfile*> profiles;
        for (PkXmlElement e = doc.documentElement().firstChildElement("defs"); !e.isNull(); e = e.nextSiblingElement("defs")) {
            for (PkXmlElement profileEl = e.firstChildElement("color-profile"); !profileEl.isNull(); profileEl = profileEl.nextSiblingElement("color-profile")) {
                const PkString href = profileEl.attribute("xlink:href");
                const PkByteArray uniqueId = pkFromHex(profileEl.attribute("local"));
                const PkString name = profileEl.attribute("name");

                const KoColorProfile *profile =
                        KoColorSpaceRegistry::instance()->profileByUniqueId(uniqueId);
                if (!profile) {
                    std::error_code ec;
                    const std::uintmax_t sz = std::filesystem::file_size(href.PkToUtf8(), ec);
                    if (!ec && sz > 0) {
                        KoColorSpaceEngine *engine = KoColorSpaceEngineRegistry::instance()->get("icc");
                        KIS_ASSERT(engine);
                        profile = engine->addProfile(href);
                    }
                }


                if (profile && !profiles.contains(name)) {
                    profiles.insert(name, profile);
                }
            }
        }
        for (PkXmlNode n = doc.documentElement().firstChild(); !n.isNull(); n = n.nextSibling()) {
            PkXmlElement e = n.toElement();

            if (e.isNull()) continue;

            if (e.tagName() == "linearGradient" || e.tagName() == "radialGradient") {
                parseSvgGradient(e, profiles);
                return;
            }
            // Inkscape gradients are in another defs
            if (e.tagName() == "defs") {


                for (PkXmlNode defnode = e.firstChild(); !defnode.isNull(); defnode = defnode.nextSibling()) {
                    PkXmlElement defelement = defnode.toElement();

                    if (defelement.isNull()) continue;

                    if (defelement.tagName() == "linearGradient" || defelement.tagName() == "radialGradient") {
                        parseSvgGradient(defelement, profiles);
                        return;
                    }
                }
            }
        }
    }
}


void KoStopGradient::parseSvgGradient(const PkXmlElement& element, PkHash<PkString, const KoColorProfile *> profiles)
{
    m_stops.clear();
    m_hasVariableStops = false;
    setSpread(PkGradientEnums::PadSpread);

    /*PkString href = e.attribute( "xlink:href" ).mid( 1 );
    if( !href.isEmpty() )
    {
    }*/
    setName(element.attribute("id", PkString("SVG Gradient")));

    bool bbox = element.attribute("gradientUnits") != "userSpaceOnUse";

    if (element.tagName() == "linearGradient") {

        if (bbox) {
            PkString s;

            s = element.attribute("x1", "0%");
            qreal xOrigin;
            if (!s.isEmpty() && s.at(s.size() - 1) == u'%')
                xOrigin = pkRemoveAll(s, '%').toDouble();
            else
                xOrigin = s.toDouble() * 100.0;

            s = element.attribute("y1", "0%");
            qreal yOrigin;
            if (!s.isEmpty() && s.at(s.size() - 1) == u'%')
                yOrigin = pkRemoveAll(s, '%').toDouble();
            else
                yOrigin = s.toDouble() * 100.0;

            s = element.attribute("x2", "100%");
            qreal xVector;
            if (!s.isEmpty() && s.at(s.size() - 1) == u'%')
                xVector = pkRemoveAll(s, '%').toDouble();
            else
                xVector = s.toDouble() * 100.0;

            s = element.attribute("y2", "0%");
            qreal yVector;
            if (!s.isEmpty() && s.at(s.size() - 1) == u'%')
                yVector = pkRemoveAll(s, '%').toDouble();
            else
                yVector = s.toDouble() * 100.0;

            m_start = PkPointF(xOrigin, yOrigin);
            m_stop = PkPointF(xVector, yVector);
        }
        else {
            m_start = PkPointF(element.attribute("x1").toDouble(), element.attribute("y1").toDouble());
            m_stop = PkPointF(element.attribute("x2").toDouble(), element.attribute("y2").toDouble());
        }
        setType(PkGradientEnums::LinearGradient);
    }
    else {
        if (bbox) {
            PkString s;

            s = element.attribute("cx", "50%");
            qreal xOrigin;
            if (!s.isEmpty() && s.at(s.size() - 1) == u'%')
                xOrigin = pkRemoveAll(s, '%').toDouble();
            else
                xOrigin = s.toDouble() * 100.0;

            s = element.attribute("cy", "50%");
            qreal yOrigin;
            if (!s.isEmpty() && s.at(s.size() - 1) == u'%')
                yOrigin = pkRemoveAll(s, '%').toDouble();
            else
                yOrigin = s.toDouble() * 100.0;

            s = element.attribute("cx", "50%");
            qreal xVector;
            if (!s.isEmpty() && s.at(s.size() - 1) == u'%')
                xVector = pkRemoveAll(s, '%').toDouble();
            else
                xVector = s.toDouble() * 100.0;

            s = element.attribute("r", "50%");
            if (!s.isEmpty() && s.at(s.size() - 1) == u'%')
                xVector += pkRemoveAll(s, '%').toDouble();
            else
                xVector += s.toDouble() * 100.0;

            s = element.attribute("cy", "50%");
            qreal yVector;
            if (!s.isEmpty() && s.at(s.size() - 1) == u'%')
                yVector = pkRemoveAll(s, '%').toDouble();
            else
                yVector = s.toDouble() * 100.0;

            s = element.attribute("fx", "50%");
            qreal xFocal;
            if (!s.isEmpty() && s.at(s.size() - 1) == u'%')
                xFocal = pkRemoveAll(s, '%').toDouble();
            else
                xFocal = s.toDouble() * 100.0;

            s = element.attribute("fy", "50%");
            qreal yFocal;
            if (!s.isEmpty() && s.at(s.size() - 1) == u'%')
                yFocal = pkRemoveAll(s, '%').toDouble();
            else
                yFocal = s.toDouble() * 100.0;

            m_start = PkPointF(xOrigin, yOrigin);
            m_stop = PkPointF(xVector, yVector);
            m_focalPoint = PkPointF(xFocal, yFocal);
        }
        else {
            m_start = PkPointF(element.attribute("cx").toDouble(), element.attribute("cy").toDouble());
            m_stop = PkPointF(element.attribute("cx").toDouble() + element.attribute("r").toDouble(),
                element.attribute("cy").toDouble());
            m_focalPoint = PkPointF(element.attribute("fx").toDouble(), element.attribute("fy").toDouble());
        }
        setType(PkGradientEnums::RadialGradient);
    }
    // handle spread method
    PkString spreadMethod = element.attribute("spreadMethod");
    if (!spreadMethod.isEmpty()) {
        if (spreadMethod == "reflect")
            setSpread(PkGradientEnums::ReflectSpread);
        else if (spreadMethod == "repeat")
            setSpread(PkGradientEnums::RepeatSpread);
    }

    for (PkXmlNode n = element.firstChild(); !n.isNull(); n = n.nextSibling()) {
        PkXmlElement colorstop = n.toElement();
        if (colorstop.tagName() == "stop") {
            qreal opacity = 0.0;
            KoColor color;
            float off;
            PkString temp = colorstop.attribute("offset");
            if (temp.contains("%")) {   // PkString::contains 收 PkString&；原 Qt 的字符字面量改字符串字面量
                temp = temp.left(temp.size() - 1);
                // 原: temp.toFloat() / 100.0 —— toFloat 先截到 float，再按 double 除法
                const float f = static_cast<float>(temp.toDouble());
                off = static_cast<float>(f / 100.0);
            }
            else
                off = static_cast<float>(temp.toDouble());

            if (!colorstop.attribute("stop-color").isEmpty())
                color = KoColor::fromSVG11(colorstop.attribute("stop-color"), profiles);
            else {
                // try style attr
                PkString style = pkSimplified(colorstop.attribute("style"));
                PkStringList substyles = pkSplitSkipEmpty(style, u';');
                for (const PkString & s : substyles) {
                    PkStringList substyle;
                    for (const PkString &part : s.split(u':')) {
                        substyle.push_back(part);
                    }
                    PkString command = substyle[0].trimmed();
                    PkString params = substyle[1].trimmed();
                    if (command == "stop-color")
                        color = KoColor::fromSVG11(params, profiles);
                    if (command == "stop-opacity")
                        opacity = params.toDouble();
                }

            }
            if (!colorstop.attribute("stop-opacity").isEmpty())
                opacity = colorstop.attribute("stop-opacity").toDouble();

            color.setOpacity(static_cast<quint8>(std::lround(opacity * OPACITY_OPAQUE_U8)));
            PkString stopTypeStr = colorstop.attribute("krita:stop-type", "color-stop");
            KoGradientStopType stopType = KoGradientStop::typeFromString(stopTypeStr);
            if (stopType != COLORSTOP) {
                m_hasVariableStops = true;
            }
            //According to the SVG spec each gradient offset has to be equal to or greater than the previous one
            //if not it needs to be adjusted to be equal
            if (m_stops.count() > 0 && m_stops.last().position >= off) {
                off = m_stops.last().position;
            }
            m_stops.append(KoGradientStop(off, color, stopType));
        }
    }
    if (m_stops.count() >= 2) {
        setValid(true);
    } else {
        setValid(false);
    }
}

PkString KoStopGradient::defaultFileExtension() const
{
    return PkString(".svg");
}

void KoStopGradient::toXML(PkXmlDocument& doc, PkXmlElement& gradientElt) const
{
    gradientElt.setAttribute("type", "stop");
    for (int s = 0; s < m_stops.size(); s++) {
        KoGradientStop stop = m_stops.at(s);
        PkXmlElement stopElt = doc.createElement("stop");
        stopElt.setAttribute("offset", KisDomUtils::toString(stop.position));
        stopElt.setAttribute("bitdepth", stop.color.colorSpace()->colorDepthId().id());
        stopElt.setAttribute("alpha", KisDomUtils::toString(stop.color.opacityF()));
        stopElt.setAttribute("stoptype", KisDomUtils::toString(stop.type));
        stop.color.toXML(doc, stopElt);
        gradientElt.appendChild(stopElt);
    }
}

KoStopGradient KoStopGradient::fromXML(const PkXmlElement& elt)
{
    KoStopGradient gradient;
    PkList<KoGradientStop> stops;
    PkXmlElement stopElt = elt.firstChildElement("stop");
    while (!stopElt.isNull()) {
        qreal offset = KisDomUtils::toDouble(stopElt.attribute("offset", "0.0"));
        PkString bitDepth = stopElt.attribute("bitdepth", Integer8BitsColorDepthID.id());
        KoColor color = KoColor::fromXML(stopElt.firstChildElement(), bitDepth);
        color.setOpacity(KisDomUtils::toDouble(stopElt.attribute("alpha", "1.0")));
        KoGradientStopType stoptype = static_cast<KoGradientStopType>(KisDomUtils::toInt(stopElt.attribute("stoptype", "0")));
        stops.append(KoGradientStop(offset, color, stoptype));
        stopElt = stopElt.nextSiblingElement("stop");
    }
    gradient.setStops(stops);
    return gradient;
}

PkString KoStopGradient::saveSvgGradient() const
{
    PkXmlDocument doc;

    doc.setContent(PkString("<svg xmlns:xlink=\"http://www.w3.org/1999/xlink\" xmlns=\"http://www.w3.org/2000/svg\" xmlns:krita=\"%1\" > </svg>").arg(KoXmlNS::krita));

    const PkString spreadMethod[3] = {
        PkString("pad"),
        PkString("reflect"),
        PkString("repeat")
    };

    PkXmlElement gradient = doc.createElement("linearGradient");
    gradient.setAttribute("id", name());
    gradient.setAttribute("gradientUnits", "objectBoundingBox");
    gradient.setAttribute("spreadMethod", spreadMethod[spread()]);

    PkHash<PkString, const KoColorProfile*> profiles;
    for(const KoGradientStop & stop: m_stops) {
        PkXmlElement stopEl = doc.createElement("stop");
        stopEl.setAttribute("stop-color", stop.color.toSVG11(&profiles));
        stopEl.setAttribute("offset", KisDomUtils::numberToString(stop.position, 6));
        stopEl.setAttribute("stop-opacity", KisDomUtils::numberToString(stop.color.opacityF(), 17));
        stopEl.setAttribute("krita:stop-type", stop.typeString());
        gradient.appendChild(stopEl);
    }

    if (profiles.size()>0) {
        PkXmlElement defs = doc.createElement("defs");
        for (PkString key: profiles.keys()) {
            const KoColorProfile * profile = profiles.value(key);

            PkXmlElement profileEl = doc.createElement("color-profile");
            profileEl.setAttribute("name", key);
            PkString val = pkToHex(profile->uniqueId());
            profileEl.setAttribute("local", val);
            profileEl.setAttribute("xlink:href", profile->fileName());
            defs.appendChild(profileEl);
        }
        doc.documentElement().appendChild(defs);
    }

    doc.documentElement().appendChild(gradient);

    return doc.toString();
}

bool KoStopGradient::saveToDevice(PkStream* dev) const
{
    PkTextStream stream(dev);
    // setUtf8OnStream 无 Qt 世界为空操作（PkTextStream 原生 UTF-8），不引 libs/global/KisPortingUtils.h
    stream << saveSvgGradient();

    return true;
}
