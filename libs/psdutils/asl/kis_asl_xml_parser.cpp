/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_asl_xml_parser.h"

#include <stdexcept>
#include <string>

#include <PkXmlDocument.h>
#include <PkStream.h>
#include <PkCosMemoryStream.h>

#include <PkColor.h>
#include "kis_asl_byte_utils.h"

#include <KoColorSpaceRegistry.h>
#include <KoColorConversions.h>
#include <resources/KoSegmentGradient.h>

#include "kis_dom_utils.h"

#include "compression.h"
#include "kis_debug.h"
#include "psd.h"
#include "psd_utils.h"

#include "kis_asl_object_catcher.h"

namespace Private
{
void parseElement(const PkXmlElement &el, const PkString &parentPath, KisAslObjectCatcher &catcher);

class CurveObjectCatcher : public KisAslObjectCatcher
{
public:
    void addText(const PkString &path, const PkString &value) override
    {
        if (path == "/Nm  ") {
            m_name = value;
        } else {
            warnKrita << "XML (ASL): failed to parse curve object" << path.PkToUtf8().c_str() << value.PkToUtf8().c_str();
        }
    }

    void addPoint(const PkString &path, const PkPointF &value) override
    {
        if (!m_arrayMode) {
            warnKrita << "XML (ASL): failed to parse curve object (array fault)" << path.PkToUtf8().c_str() << value << ppVar(m_arrayMode);
        }

        m_points.append(value);
    }

public:
    PkVector<PkPointF> m_points;
    PkString m_name;
};

KoColor parseColorObject(PkXmlElement parent, PkString classID)
{
    KoColor color;
    KoColor error = KoColor::fromXML("<color channeldepth='U8'><sRGB r='1.0' g='0.0' b='0.0'/></color>");
    PkXmlDocument doc;
    PkXmlElement root;
    PkString spotBook;
    PkString spotName;
    int spotValue = 0;
    double h = 0;
    double s = 0;
    double v= 0;

    if (classID == "RGBC" || classID == "HSBC") {
        color = KoColor(KoColorSpaceRegistry::instance()->rgb8());
        root = doc.createElement("sRGB");
    } else if (classID == "CMYC") {
        root = doc.createElement("CMYK");
    } else if (classID == "LbCl") {
        root = doc.createElement("Lab");
    } else if (classID == "Grsc") {
        root = doc.createElement("Gray");
    } else {
        // Can be 'UnsC', or something else.
        warnKrita << "Unknown color type:" << "classID" << "=" << classID.PkToUtf8().c_str();
        return error;
    }

    PkXmlNode child = parent.firstChild();
    while (!child.isNull()) {
        PkXmlElement childEl = child.toElement();

        PkString type = childEl.attribute("type", "<unknown>");
        PkString key = childEl.attribute("key", "");

        if (type == "Double" || type == "UnitFloat") {
            if (classID == "RGBC") {
                // For RGBC we'll just directly write to the KoColor data, to have as
                // few rounding errors possible.
                double value = KisDomUtils::toDouble(childEl.attribute("value", "0"));

                if (key == "Rd  ") {
                    color.data()[2] = value;
                } else if (key == "Grn ") {
                    color.data()[1] = value;
                } else if (key == "Bl  ") {
                    color.data()[0] = value;
                } else {
                    warnKrita << "Unknown color key value double:" << "key" << "=" << key.PkToUtf8().c_str();
                    return error;
                }
            } else if (classID == "CMYC") {
                double value = KisDomUtils::toDouble(childEl.attribute("value", "0")) * 0.01;
                // CMYK is stored in percentages...
                if (key == "Cyn ") {
                    root.setAttribute("c", KisDomUtils::toString(value));
                } else if (key == "Mgnt") {
                    root.setAttribute("m", KisDomUtils::toString(value));
                } else if (key == "Ylw ") {
                    root.setAttribute("y", KisDomUtils::toString(value));
                } else if (key == "Blck") {
                    root.setAttribute("k", KisDomUtils::toString(value));
                } else {
                    warnKrita << "Unknown color key value double:" << "key" << "=" << key.PkToUtf8().c_str();
                    return error;
                }
            } else if (classID == "LbCl") {
                if (key == "Lmnc") {
                    root.setAttribute("L", childEl.attribute("value", "0"));
                } else if (key == "A   ") {
                    root.setAttribute("a", childEl.attribute("value", "0"));
                } else if (key == "B   ") {
                    root.setAttribute("b", childEl.attribute("value", "0"));
                } else {
                    warnKrita << "Unknown color key value:" << "key" << "=" << key.PkToUtf8().c_str();
                    return error;
                }
            } else if (classID == "Grsc") {
                // Unsure that grey is stored as a percentage, might also be 255.
                double value = KisDomUtils::toDouble(childEl.attribute("value", "0")) * 0.01;
                if (key == "Gry ") {
                    root.setAttribute("g", KisDomUtils::toString(value));
                } else {
                    warnKrita << "Unknown color key value:" << "key" << "=" << key.PkToUtf8().c_str();
                    return error;
                }
            } else if (classID == "HSBC") {
                double value = KisDomUtils::toDouble(childEl.attribute("value", "0"));
                if (key == "H   ") {
                    h = value;
                } else if (key == "Strt") {
                    s = value * 0.01;
                } else if (key == "Brgh") {
                    v = value * 0.01;
                } else {
                    warnKrita << "Unknown color key value:" << "key" << "=" << key.PkToUtf8().c_str();
                    return error;
                }
            }
        } else if (type == "Text") {
            if (key== "Bk  ") {
                spotBook = childEl.attribute("value", "");
            } else if (key== "Nm  ") {
                spotName = childEl.attribute("value", "");
            } else {
                warnKrita << "Unknown color key value string:" << "key" << "=" << key.PkToUtf8().c_str();
            }
        } else if (type == "Integer") {
            if (key== "bookID") {
                spotValue = KisDomUtils::toInt(childEl.attribute("value", "0"));
            } else {
                warnKrita << "Unknown color key value integer:" << "key" << "=" << key.PkToUtf8().c_str();
            }
        } else {
            qDebug() << "Unknown color component type:" << "type" << "=" << type.PkToUtf8().c_str()
                     << "key" << "=" << key.PkToUtf8().c_str();
            return error;
        }

        child = child.nextSibling();
    }
    if (classID == "HSBC") {
        float r = 0.0;
        float b = 0.0;
        float g = 0.0;
        HSVToRGB(h, s, v, &r, &g, &b);
        root.setAttribute("r", KisDomUtils::toString(r));
        root.setAttribute("g", KisDomUtils::toString(g));
        root.setAttribute("b", KisDomUtils::toString(b));
    }
    if (classID != "RGBC") {
        color = KoColor::fromXML(root, "U8");
    }
    color.setOpacity(OPACITY_OPAQUE_U8);
    if (!spotName.isEmpty()) {
        color.addMetadata("spotName", spotName);
        color.addMetadata("psdSpotBook", spotBook);
        color.addMetadata("psdSpotBookId", spotValue);
    }

    return color;
}

void parseColorStopsList(PkXmlElement parent,
                         PkVector<qreal> &startLocations,
                         PkVector<qreal> &middleOffsets,
                         PkVector<KoColor> &colors,
                         PkVector<KoGradientSegmentEndpointType> &types)
{
    PkXmlNode child = parent.firstChild();
    while (!child.isNull()) {
        PkXmlElement childEl = child.toElement();

        PkString type = childEl.attribute("type", "<unknown>");
        PkString key = childEl.attribute("key", "");
        PkString classId = childEl.attribute("classId", "");

        if (type == "Descriptor" && classId == "Clrt") {
            // sorry for naming...
            PkXmlNode child = childEl.firstChild();
            while (!child.isNull()) {
                PkXmlElement childEl = child.toElement();

                PkString type = childEl.attribute("type", "<unknown>");
                PkString key = childEl.attribute("key", "");
                PkString classId = childEl.attribute("classId", "");

                if (type == "Integer" && key == "Lctn") {
                    int value = KisDomUtils::toInt(childEl.attribute("value", "0"));
                    startLocations.append(qreal(value) / 4096.0);

                } else if (type == "Integer" && key == "Mdpn") {
                    int value = KisDomUtils::toInt(childEl.attribute("value", "0"));
                    middleOffsets.append(qreal(value) / 100.0);

                } else if (type == "Descriptor" && key == "Clr ") {
                    colors.append(parseColorObject(childEl, classId));

                } else if (type == "Enum" && key == "Type") {
                    PkString typeId = childEl.attribute("typeId", "");

                    if (typeId != "Clry") {
                        warnKrita << "WARNING: Invalid typeId of a gradient stop type" << typeId.PkToUtf8().c_str();
                    }

                    PkString value = childEl.attribute("value", "");
                    if (value == "BckC") {
                        types.append(BACKGROUND_ENDPOINT);
                    } else if (value == "FrgC") {
                        types.append(FOREGROUND_ENDPOINT);
                    } else {
                        types.append(COLOR_ENDPOINT);
                    }
                }

                child = child.nextSibling();
            }
        } else {
            warnKrita << "WARNING: Unrecognized object in color stops list"
                      << "type" << "=" << type.PkToUtf8().c_str()
                      << "key" << "=" << key.PkToUtf8().c_str()
                      << "classId" << "=" << classId.PkToUtf8().c_str();
        }

        child = child.nextSibling();
    }
}

void parseTransparencyStopsList(PkXmlElement parent, PkVector<qreal> &startLocations, PkVector<qreal> &middleOffsets, PkVector<qreal> &transparencies)
{
    PkXmlNode child = parent.firstChild();
    while (!child.isNull()) {
        PkXmlElement childEl = child.toElement();

        PkString type = childEl.attribute("type", "<unknown>");
        PkString key = childEl.attribute("key", "");
        PkString classId = childEl.attribute("classId", "");

        if (type == "Descriptor" && classId == "TrnS") {
            // sorry for naming again...
            PkXmlNode child = childEl.firstChild();
            while (!child.isNull()) {
                PkXmlElement childEl = child.toElement();

                PkString type = childEl.attribute("type", "<unknown>");
                PkString key = childEl.attribute("key", "");

                if (type == "Integer" && key == "Lctn") {
                    int value = KisDomUtils::toInt(childEl.attribute("value", "0"));
                    startLocations.append(qreal(value) / 4096.0);
                } else if (type == "Integer" && key == "Mdpn") {
                    int value = KisDomUtils::toInt(childEl.attribute("value", "0"));
                    middleOffsets.append(qreal(value) / 100.0);
                } else if (type == "UnitFloat" && key == "Opct") {
                    PkString unit = childEl.attribute("unit", "");
                    if (unit != "#Prc") {
                        warnKrita << "WARNING: Invalid unit of a gradient stop transparency" << unit.PkToUtf8().c_str();
                    }

                    qreal value = KisDomUtils::toDouble(childEl.attribute("value", "100"));
                    transparencies.append(value / 100.0);
                }

                child = child.nextSibling();
            }

        } else {
            warnKrita << "WARNING: Unrecognized object in transparency stops list"
                      << "type" << "=" << type.PkToUtf8().c_str()
                      << "key" << "=" << key.PkToUtf8().c_str()
                      << "classId" << "=" << classId.PkToUtf8().c_str();
        }

        child = child.nextSibling();
    }
}

inline PkString buildPath(const PkString &parent, const PkString &key)
{
    return parent + "/" + key;
}

bool tryParseDescriptor(const PkXmlElement &el, const PkString &path, const PkString &classId, KisAslObjectCatcher &catcher)
{
    bool retval = true;

    if (classId == "null") {
        catcher.newStyleStarted();
        // here we just notify that a new style is started, we haven't
        // processed the whole block yet, so return false.
        retval = false;
    } else if (el.attribute("key", " ") == "Clr ") {
        catcher.addColor(path, parseColorObject(el, classId));
    } else if (el.attribute("key", " ") == "hglC" || el.attribute("key", " ") == "sdwC") {
        // like Clr, but /ebbl/ likes to do everything differently - see bug 464218
        catcher.addColor(path, parseColorObject(el, classId));
    } else if (classId == "ShpC") {
        CurveObjectCatcher curveCatcher;

        PkXmlNode child = el.firstChild();
        while (!child.isNull()) {
            parseElement(child.toElement(), "", curveCatcher);
            child = child.nextSibling();
        }

        catcher.addCurve(path, curveCatcher.m_name, curveCatcher.m_points);

    } else if (classId == "CrPt") {
        PkPointF point;

        PkXmlNode child = el.firstChild();
        while (!child.isNull()) {
            PkXmlElement childEl = child.toElement();

            PkString type = childEl.attribute("type", "<unknown>");
            PkString key = childEl.attribute("key", "");

            if (type == "Boolean" && key == "Cnty") {
                warnKrita << "WARNING: tryParseDescriptor: The points of the curve object contain \'Cnty\' flag which is unsupported by Krita";
                warnKrita << "        " << "type" << "=" << type.PkToUtf8().c_str()
                          << "key" << "=" << key.PkToUtf8().c_str()
                          << "path" << "=" << path.PkToUtf8().c_str();

                child = child.nextSibling();
                continue;
            }

            if (type != "Double") {
                warnKrita << "Unknown point component type:" << "type" << "=" << type.PkToUtf8().c_str()
                          << "key" << "=" << key.PkToUtf8().c_str()
                          << "path" << "=" << path.PkToUtf8().c_str();
                return false;
            }

            double value = KisDomUtils::toDouble(childEl.attribute("value", "0"));

            if (key == "Hrzn") {
                point.setX(value);
            } else if (key == "Vrtc") {
                point.setY(value);
            } else {
                warnKrita << "Unknown point key value:" << "key" << "=" << key.PkToUtf8().c_str()
                          << "path" << "=" << path.PkToUtf8().c_str();
                return false;
            }

            child = child.nextSibling();
        }

        catcher.addPoint(path, point);

    } else if (classId == "Pnt ") {
        PkPointF point;

        PkXmlNode child = el.firstChild();
        while (!child.isNull()) {
            PkXmlElement childEl = child.toElement();

            PkString type = childEl.attribute("type", "<unknown>");
            PkString key = childEl.attribute("key", "");
            PkString unit = childEl.attribute("unit", "");

            if (type != "Double" && !(type == "UnitFloat" && unit == "#Prc")) {
                warnKrita << "Unknown point component type:" << "unit" << "=" << unit.PkToUtf8().c_str()
                          << "type" << "=" << type.PkToUtf8().c_str()
                          << "key" << "=" << key.PkToUtf8().c_str()
                          << "path" << "=" << path.PkToUtf8().c_str();
                return false;
            }

            double value = KisDomUtils::toDouble(childEl.attribute("value", "0"));

            if (key == "Hrzn") {
                point.setX(value);
            } else if (key == "Vrtc") {
                point.setY(value);
            } else {
                warnKrita << "Unknown point key value:" << "key" << "=" << key.PkToUtf8().c_str()
                          << "path" << "=" << path.PkToUtf8().c_str();
                return false;
            }

            child = child.nextSibling();
        }

        catcher.addPoint(path, point);

    } else if (classId == "KisPattern") {
        PkByteArray patternData;
        PkString patternUuid;

        PkXmlNode child = el.firstChild();
        while (!child.isNull()) {
            PkXmlElement childEl = child.toElement();

            PkString type = childEl.attribute("type", "<unknown>");
            PkString key = childEl.attribute("key", "");

            if (type == "Text" && key == "Idnt") {
                patternUuid = childEl.attribute("value", "").trimmed();
            }

            if (type == "KisPatternData" && key == "Data") {
                PkXmlNode dataNode = child.firstChild();

                if (!dataNode.isCDATASection()) {
                    warnKrita << "WARNING: failed to parse KisPatternData XML section!";
                    continue;
                }

                PkXmlCDATASection dataSection = dataNode.toCDATASection();
                // CDATA 内容是 base64 的 ASCII 串：fromBase64 直接收 PkString。
                PkByteArray data = pkFromBase64(dataSection.data());
                data = pkQUncompress(data);

                if (data.isEmpty()) {
                    warnKrita << "WARNING: failed to parse KisPatternData XML section!";
                    continue;
                }

                patternData = data;
            }

            child = child.nextSibling();
        }

        if (!patternUuid.isEmpty() && !patternData.isEmpty()) {
            PkString fileName = PkString("%1.pat").arg(patternUuid);

            PkSharedPointer<KoPattern> pattern(new KoPattern(fileName));

            PkCosMemoryStream buffer(&patternData);
            buffer.open(PkStream::ReadOnly);

            if (pattern->loadPatFromDevice(&buffer) && pattern->valid()) {
                catcher.addPattern(path, pattern, patternUuid);
            }
            else {
                warnKrita << "WARNING: failed to create pattern:" << "patternUuid" << "=" << patternUuid.PkToUtf8().c_str() << ppVar(pattern);
            }
        } else {
            warnKrita << "WARNING: failed to load KisPattern XML section!" << "patternUuid" << "=" << patternUuid.PkToUtf8().c_str();
        }

    } else if (classId == "Ptrn") { // reference to an existing pattern
        PkString patternUuid;
        PkString patternName;

        PkXmlNode child = el.firstChild();
        while (!child.isNull()) {
            PkXmlElement childEl = child.toElement();

            PkString type = childEl.attribute("type", "<unknown>");
            PkString key = childEl.attribute("key", "");

            if (type == "Text" && key == "Idnt") {
                patternUuid = childEl.attribute("value", "");
            } else if (type == "Text" && key == "Nm  ") {
                patternName = childEl.attribute("value", "");
            } else {
                warnKrita << "WARNING: unrecognized pattern-ref section key:" << "type" << "=" << type.PkToUtf8().c_str()
                          << "key" << "=" << key.PkToUtf8().c_str();
            }

            child = child.nextSibling();
        }

        catcher.addPatternRef(path, patternUuid, patternName);

    } else if (classId == "Grdn") {
        PkString gradientName;
        qreal gradientSmoothness = 100.0;

        PkVector<qreal> startLocations;
        PkVector<qreal> middleOffsets;
        PkVector<KoColor> colors;
        PkVector<KoGradientSegmentEndpointType> types;

        PkVector<qreal> transpStartLocations;
        PkVector<qreal> transpMiddleOffsets;
        PkVector<qreal> transparencies;

        PkXmlNode child = el.firstChild();
        while (!child.isNull()) {
            PkXmlElement childEl = child.toElement();

            PkString type = childEl.attribute("type", "<unknown>");
            PkString key = childEl.attribute("key", "");

            if (type == "Text" && key == "Nm  ") {
                gradientName = childEl.attribute("value", "");
            } else if (type == "Enum" && key == "GrdF") {
                PkString typeId = childEl.attribute("typeId", "");
                PkString value = childEl.attribute("value", "");

                if (typeId != "GrdF" || value != "CstS") {
                    warnKrita << "WARNING: Unsupported gradient type (probably, noise-based):" << value.PkToUtf8().c_str();
                    return true;
                }
            } else if (type == "Double" && key == "Intr") {
                double value = KisDomUtils::toDouble(childEl.attribute("value", "4096"));
                gradientSmoothness = 100.0 * value / 4096.0;
            } else if (type == "List" && key == "Clrs") {
                parseColorStopsList(childEl, startLocations, middleOffsets, colors, types);
            } else if (type == "List" && key == "Trns") {
                parseTransparencyStopsList(childEl, transpStartLocations, transpMiddleOffsets, transparencies);
            }

            child = child.nextSibling();
        }

        if (colors.size() < transparencies.size()) {
            const KoColor lastColor = !colors.isEmpty() ? colors.last() : KoColor();
            const KoGradientSegmentEndpointType lastType = !types.isEmpty() ? types.last() : COLOR_ENDPOINT;
            while (colors.size() != transparencies.size()) {
                const int index = colors.size();
                colors.append(lastColor);
                startLocations.append(transpStartLocations[index]);
                middleOffsets.append(transpMiddleOffsets[index]);
                types.append(lastType);
            }
        }

        if (colors.size() > transparencies.size()) {
            const qreal lastTransparency = !transparencies.isEmpty() ? transparencies.last() : 1.0;
            while (colors.size() != transparencies.size()) {
                const int index = transparencies.size();
                transparencies.append(lastTransparency);
                transpStartLocations.append(startLocations[index]);
                transpMiddleOffsets.append(middleOffsets[index]);
            }
        }

        if (colors.size() == 1) {
            colors.append(colors.last());
            startLocations.append(1.0);
            middleOffsets.append(0.5);
            types.append(COLOR_ENDPOINT);

            transparencies.append(transparencies.last());
            transpStartLocations.append(1.0);
            transpMiddleOffsets.append(0.5);
        }

        /**
         * Filenames in Krita cannot have slashes inside, but some of the
         * styles saved in 4.x days could have that. Here we just forcefully
         * crop the directory part of the gradient to make sure that it fits
         * the new policy.
         *
         * Since ASL doesn't use this name as any linkage (actually, gradients
         * are always embedded into the style) so we don't really care about
         * the contents of the filename field. It should just be somewhat unique.
         */
        // 原实现按路径取文件名，语义：取路径最后一个 '/' 之后的部分。
        // PkString 无 lastIndexOf，用 split 取末段等价。
        const std::vector<PkString> pathParts = gradientName.split(u'/');
        const PkString baseName = pathParts.empty() ? PkString() : pathParts.back();
        const PkString fileName = baseName + ".ggr";
        PkSharedPointer<KoSegmentGradient> gradient(new KoSegmentGradient(fileName));
        Q_UNUSED(gradientSmoothness);
        gradient->setName(gradientName);

        if (colors.size() >= 2) {
            for (int i = 1; i < colors.size(); i++) {
                KoColor startColor = colors[i - 1];
                KoColor endColor = colors[i];
                startColor.setOpacity(transparencies[i - 1]);
                endColor.setOpacity(transparencies[i]);

                qreal start = startLocations[i - 1];
                qreal end = startLocations[i];
                qreal middle = start + middleOffsets[i - 1] * (end - start);

                KoGradientSegmentEndpointType startType = types[i - 1];
                KoGradientSegmentEndpointType endType = types[i];

                gradient->createSegment(INTERP_LINEAR, COLOR_INTERP_RGB, start, end, middle, startColor, endColor, startType, endType);
            }
            gradient->setValid(true);
            gradient->updatePreview();
        } else {
            gradient->setValid(false);
        }

        catcher.addGradient(path, gradient);
    } else if (classId == "Trnf") {

        double xx = 1.0;
        double xy = 0.0;
        double yx = 0.0;
        double yy = 1.0;
        double tx = 0.0;
        double ty = 0.0;

        PkXmlNode child = el.firstChild();
        while (!child.isNull()) {
            PkXmlElement childEl = child.toElement();

            PkString type = childEl.attribute("type", "<unknown>");
            PkString key = childEl.attribute("key", "");
            double value = KisDomUtils::toDouble(childEl.attribute("value"));


            if (type == "Double") {
                if (key == "xx") {
                    xx = value;
                } else if (key == "xy") {
                    xy = value;
                } else if (key == "yx") {
                    yx = value;
                } else if (key == "yy") {
                    yy = value;
                } else if (key == "tx") {
                    tx = value;
                } else if (key == "ty") {
                    ty = value;
                }
            }

            child = child.nextSibling();
        }
        catcher.addTransform(path, PkTransform(xx, xy, yx, yy, tx, ty));
    } else if (classId == "classFloatRect") {

        PkRectF rect;

        PkXmlNode child = el.firstChild();
        while (!child.isNull()) {
            PkXmlElement childEl = child.toElement();

            PkString type = childEl.attribute("type", "<unknown>");
            PkString key = childEl.attribute("key", "");
            double value = KisDomUtils::toDouble(childEl.attribute("value"));


            if (type == "Double") {
                if (key == "Top ") {
                    rect.setTop(value);
                } else if (key == "Left") {
                    rect.setLeft(value);
                } else if (key == "Btom") {
                    rect.setBottom(value);
                } else if (key == "Rght") {
                    rect.setRight(value);
                }
            }
            child = child.nextSibling();
        }

        if (el.attribute("key", " ") == "keyOriginShapeBBox") {
            catcher.addUnitRect(path, "#Pxl", rect);
        } else {
            catcher.addRect(path, rect);
        }
    } else if (classId == "unitRect") {
        PkRectF rect;

        PkXmlNode child = el.firstChild();
        PkString unit;
        while (!child.isNull()) {
            PkXmlElement childEl = child.toElement();

            PkString type = childEl.attribute("type", "<unknown>");
            PkString key = childEl.attribute("key", "");
            unit = childEl.attribute("unit", unit);
            double value = KisDomUtils::toDouble(childEl.attribute("value"));


            if (type == "UnitFloat") {
                if (key == "Top ") {
                    rect.setTop(value);
                } else if (key == "Left") {
                    rect.setLeft(value);
                } else if (key == "Btom") {
                    rect.setBottom(value);
                } else if (key == "Rght") {
                    rect.setRight(value);
                }
            }
            child = child.nextSibling();
        }

        catcher.addUnitRect(path, unit, rect);

    } else {
        retval = false;
    }

    return retval;
}

void parseElement(const PkXmlElement &el, const PkString &parentPath, KisAslObjectCatcher &catcher)
{
    KIS_ASSERT_RECOVER_RETURN(el.tagName() == "node");

    PkString type = el.attribute("type", "<unknown>");
    PkString key = el.attribute("key", "");

    if (type == "Descriptor") {
        PkString classId = el.attribute("classId", "<noClassId>");
        PkString containerName = key.isEmpty() ? classId : key;
        PkString containerPath = buildPath(parentPath, containerName);

        if (!tryParseDescriptor(el, containerPath, classId, catcher)) {
            PkXmlNode child = el.firstChild();
            while (!child.isNull()) {
                parseElement(child.toElement(), containerPath, catcher);
                child = child.nextSibling();
            }
        }
    } else if (type == "List") {
        catcher.setArrayMode(true);

        PkString containerName = key;
        PkString containerPath = buildPath(parentPath, containerName);

        PkXmlNode child = el.firstChild();
        while (!child.isNull()) {
            parseElement(child.toElement(), containerPath, catcher);
            child = child.nextSibling();
        }

        catcher.setArrayMode(false);
    } else if (type == "Double") {
        double v = KisDomUtils::toDouble(el.attribute("value", "0"));
        catcher.addDouble(buildPath(parentPath, key), v);
    } else if (type == "UnitFloat") {
        PkString unit = el.attribute("unit", "<unknown>");
        double v = KisDomUtils::toDouble(el.attribute("value", "0"));
        catcher.addUnitFloat(buildPath(parentPath, key), unit, v);
    } else if (type == "Text") {
        PkString v = el.attribute("value", "");
        catcher.addText(buildPath(parentPath, key), v);
    } else if (type == "Enum") {
        PkString v = el.attribute("value", "");
        PkString typeId = el.attribute("typeId", "<unknown>");
        catcher.addEnum(buildPath(parentPath, key), typeId, v);
    } else if (type == "Integer") {
        int v = KisDomUtils::toInt(el.attribute("value", "0"));
        catcher.addInteger(buildPath(parentPath, key), v);
    } else if (type == "Boolean") {
        int v = KisDomUtils::toInt(el.attribute("value", "0"));
        catcher.addBoolean(buildPath(parentPath, key), v);
    } else if (type == "RawData") {
        PkXmlNode dataNode = el.firstChild();

        if (!dataNode.isCDATASection()) {
            warnKrita << "WARNING: failed to parse RawData XML section!";
            return;
        }

        PkXmlCDATASection dataSection = dataNode.toCDATASection();
        // CDATA 内容是 base64 的 ASCII 串：fromBase64 直接收 PkString。
        PkByteArray data = pkFromBase64(dataSection.data());

        if (data.isEmpty()) {
            warnKrita << "WARNING: failed to parse RawData XML section!";
        }
        catcher.addRawData(buildPath(parentPath, key), data);
    } else {
        warnKrita << "WARNING: XML (ASL) Unknown element type:" << type.PkToUtf8().c_str()
                  << "parentPath" << "=" << parentPath.PkToUtf8().c_str()
                  << "key" << "=" << key.PkToUtf8().c_str();
    }
}

} // namespace

void KisAslXmlParser::parseXML(const PkXmlDocument &doc, KisAslObjectCatcher &catcher)
{
    PkXmlElement root = doc.documentElement();
    if (root.tagName() != "asl") {
        return;
    }

    PkXmlNode child = root.firstChild();
    while (!child.isNull()) {
        Private::parseElement(child.toElement(), "", catcher);
        child = child.nextSibling();
    }
}
