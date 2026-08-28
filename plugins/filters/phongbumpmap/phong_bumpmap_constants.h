/*
 *  SPDX-FileCopyrightText: 2010 José Luis Vergara <pentalis@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CONSTANTS_H
#define CONSTANTS_H

/*
* Illuminants were renamed to Light Sources in the GUI
*/
const quint8 PHONG_TOTAL_ILLUMINANTS = 4;
const PkString PHONG_HEIGHT_CHANNEL = "heightChannel";
const PkString USE_NORMALMAP_IS_ENABLED = "useNormalMapIsEnabled";
const PkString PHONG_ILLUMINANT_IS_ENABLED[] =
{"illuminantIsEnabled0",
"illuminantIsEnabled1",
"illuminantIsEnabled2",
"illuminantIsEnabled3"};
const PkString PHONG_ILLUMINANT_COLOR[] =
{"illuminantColor0",
"illuminantColor1",
"illuminantColor2",
"illuminantColor3"};
const PkString PHONG_ILLUMINANT_AZIMUTH[] =
{"Azimuth0",
"Azimuth1",
"Azimuth2",
"Azimuth3"};
const PkString PHONG_ILLUMINANT_INCLINATION[] =
{"Inclination0",
"Inclination1",
"Inclination2",
"Inclination3"};
const PkString PHONG_AMBIENT_REFLECTIVITY = "ambientReflectivity";
const PkString PHONG_DIFFUSE_REFLECTIVITY = "diffuseReflectivity";
const PkString PHONG_SPECULAR_REFLECTIVITY = "specularReflectivity";
const PkString PHONG_SHINYNESS_EXPONENT = "shinynessExponent";
const PkString PHONG_DIFFUSE_REFLECTIVITY_IS_ENABLED = "diffuseReflectivityIsEnabled";
const PkString PHONG_SPECULAR_REFLECTIVITY_IS_ENABLED = "specularReflectivityIsEnabled";
//const PkString PHONG_SHINYNESS_EXPONENT_IS_ENABLED = "shinynessExponentIsEnabled";

#endif
