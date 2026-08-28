/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "../multigridpatternhelpers.h"

#include <PkXmlDocument.h>

#include <iostream>

namespace {

bool sameShape(const PkPolygonF &left, const PkPolygonF &right)
{
    if (left.size() != right.size()) {
        return false;
    }
    for (int i = 0; i < left.size(); ++i) {
        if (left.at(i) != right.at(i)) {
            return false;
        }
    }
    return true;
}

}

int main()
{
    const PkString xml = multigridDefaultGradientXml();
    if (xml.isEmpty()) {
        std::cerr << "default gradient XML is empty\n";
        return 1;
    }

    PkXmlDocument gradientDocument;
    if (!gradientDocument.setContent(xml)) {
        std::cerr << "default gradient XML is invalid\n";
        return 1;
    }
    const PkXmlElement gradient = gradientDocument.documentElement();
    const PkXmlElement greenStop = gradient.firstChildElement("stop");
    const PkXmlElement green = greenStop.firstChildElement("RGB");
    const PkXmlElement blueStop = greenStop.nextSiblingElement("stop");
    const PkXmlElement blue = blueStop.firstChildElement("RGB");
    if (gradient.attribute("type") != "stop" ||
        greenStop.attribute("offset") != "0" ||
        greenStop.attribute("bitdepth") != "U8" ||
        greenStop.attribute("alpha") != "1" ||
        greenStop.attribute("stoptype") != "0" ||
        green.attribute("r") != "0" || green.attribute("g") != "1" ||
        green.attribute("b") != "0" ||
        green.attribute("space") != "sRGB-elle-V2-srgbtrc.icc" ||
        blueStop.attribute("offset") != "1" ||
        blue.attribute("r") != "0" || blue.attribute("g") != "0" ||
        blue.attribute("b") != "1" ||
        !blueStop.nextSiblingElement("stop").isNull()) {
        std::cerr << "default gradient is not the legacy green-to-blue gradient: "
                  << xml.PkToUtf8() << '\n';
        return 1;
    }

    const PkList<KisMultiGridRhomb> first = generateMultigridRhombs(5, 2, 0.2);
    const PkList<KisMultiGridRhomb> second = generateMultigridRhombs(5, 2, 0.2);
    if (first.isEmpty() || first.size() != second.size() ||
        !sameShape(first.first().shape, second.first().shape)) {
        std::cerr << "multigrid geometry is empty or non-deterministic\n";
        return 1;
    }
    return 0;
}
