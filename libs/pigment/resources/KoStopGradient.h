/*
    SPDX-FileCopyrightText: 2007 Sven Langkamp <sven.langkamp@gmail.com>

    SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef KOSTOPGRADIENT_H
#define KOSTOPGRADIENT_H

#include <PkPair.h>
#include <PkGradient.h>
#include <PkList.h>
#include <PkString.h>
#include <PkHash.h>
#include <PkSharedPointer.h>
#include <PkPoint.h>

#include "KoColor.h"
#include <resources/KoAbstractGradient.h>
#include <KoResource.h>
#include <KisResourceTypes.h>
#include <kritapigment_export.h>
#include <boost/operators.hpp>

class PkStream;
class PkXmlDocument;
class PkXmlElement;

enum KoGradientStopType
{
    COLORSTOP,
    FOREGROUNDSTOP,
    BACKGROUNDSTOP
};

struct KoGradientStop : public boost::equality_comparable<KoGradientStop>
{
    KoGradientStopType type;
    KoColor color;
    qreal position;

    KoGradientStop(qreal _position = 0.0, KoColor _color = KoColor(), KoGradientStopType _type = COLORSTOP)
    {
        type = _type;
        color = _color;
        position = _position;
    }

    bool operator == (const KoGradientStop& other) const
    {
        return this->type == other.type && this->color == other.color && this->position == other.position;
    }



    PkString typeString() const
    {
        switch (type) {
        case COLORSTOP:
            return "color-stop";
        case FOREGROUNDSTOP:
            return "foreground-stop";
        case BACKGROUNDSTOP:
            return "background-stop";
        default:
            return "color-stop";
        }
    }

    static KoGradientStopType typeFromString(PkString typestring) {
        if (typestring == "foreground-stop") {
            return FOREGROUNDSTOP;
        } else if (typestring == "background-stop") {
            return BACKGROUNDSTOP;
        } else {
            return COLORSTOP;
        }
    }
};


struct KoGradientStopValueSort
{
    inline bool operator() (const KoGradientStop& a, const KoGradientStop& b) {
        return (a.color.toQColor().value() < b.color.toQColor().value());
    }
};

struct KoGradientStopHueSort
{
    inline bool operator() (const KoGradientStop& a, const KoGradientStop& b) {
        return (a.color.toQColor().hue() < b.color.toQColor().hue());
    }
};

/**
 * Resource for colorstop based gradients like SVG gradients
 */
class KRITAPIGMENT_EXPORT KoStopGradient : public KoAbstractGradient, public boost::equality_comparable<KoStopGradient>
{

public:

    explicit KoStopGradient(const PkString &filename = PkString());
    ~KoStopGradient() override;
    KoStopGradient(const KoStopGradient &rhs);
    bool operator==(const KoStopGradient &rhs) const;
    KoStopGradient &operator=(const KoStopGradient &rhs) = delete;
    KoResourceSP clone() const override;

    bool loadFromDevice(PkStream *dev, KisResourcesInterfaceSP resourcesInterface) override;
    bool saveToDevice(PkStream* dev) const override;

    PkPair<PkString, PkString> resourceType() const override {
        return PkPair<PkString, PkString>(ResourceType::Gradients, ResourceSubType::StopGradients);
    }

    /// reimplemented
    PkGradient* toQGradient() const override;

    /// Find stops surrounding position, returns false if position outside gradient
    bool stopsAt(KoGradientStop& leftStop, KoGradientStop& rightStop, qreal t) const;

    /// reimplemented
    void colorAt(KoColor&, qreal t) const override;

    /// Creates KoStopGradient from a gradient
    static PkSharedPointer<KoStopGradient> fromQGradient(const PkGradient *gradient);

    /// Sets the gradient stops
    void setStops(PkList<KoGradientStop> stops);
    PkList<KoGradientStop> stops() const;

    PkList<int> requiredCanvasResources() const override;
    void bakeVariableColors(KoCanvasResourcesInterfaceSP canvasResourcesInterface) override;
    void updateVariableColors(KoCanvasResourcesInterfaceSP canvasResourcesInterface) override;


    /// reimplemented
    PkString defaultFileExtension() const override;

    /**
     * @brief toXML
     * Convert the gradient to an XML string.
     */
    void toXML(PkXmlDocument& doc, PkXmlElement& gradientElt) const;
    /**
     * @brief fromXML
     * convert a gradient from xml.
     * @return a gradient.
     */
    static KoStopGradient fromXML(const PkXmlElement& elt);

    PkString saveSvgGradient() const;

protected:

    PkList<KoGradientStop> m_stops;
    bool m_hasVariableStops = false;
    PkPointF m_start;
    PkPointF m_stop;
    PkPointF m_focalPoint;

private:

    void loadSvgGradient(PkStream *file);
    void parseSvgGradient(const PkXmlElement& element, PkHash<PkString, const KoColorProfile*> profiles);
};

typedef PkSharedPointer<KoStopGradient> KoStopGradientSP;

#endif // KOSTOPGRADIENT_H
