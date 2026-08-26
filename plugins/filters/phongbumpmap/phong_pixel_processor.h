/*
*  SPDX-FileCopyrightText: 2010-2011 José Luis Vergara <pentalis@gmail.com>
*
*  SPDX-License-Identifier: GPL-2.0-or-later
*/

#ifndef PHONG_PIXEL_PROCESSOR_H
#define PHONG_PIXEL_PROCESSOR_H

#include <QVector3D>
#include <PkDateTime.h>
#include <PkColor.h>
#include <PkList.h>
#include <PkMap.h>

#include "phong_bumpmap_constants.h"
#include "kis_properties_configuration.h"

struct Illuminant
{
    PkList<qreal> RGBvalue;
    QVector3D lightVector;
};

class PhongPixelProcessor
{

public:
    PhongPixelProcessor(quint32 pixelArea, const KisPropertiesConfigurationSP config);
    ~PhongPixelProcessor();

    void initialize(const KisPropertiesConfigurationSP config);
    void normalizeHeightmap();

    QVector3D reflection_vector;
    QVector3D normal_vector;
    QVector3D x_vector;
    QVector3D y_vector;
    QVector3D light_vector;
    QVector3D vision_vector;

    PkVector<double> realheightmap;

    ///Ambient light coefficient
    qreal Ka;

    ///Diffuse light coefficient
    qreal Kd;

    ///Specular light coefficient
    qreal Ks;

    ///Shinyness exponent
    qreal shiny_exp;

    ///Total ambient light
    qreal Ia;

    ///Total diffuse light
    qreal Id;

    ///Total specular light
    qreal Is;

    PkVector<quint16> IlluminatePixelFromHeightmap(quint32 posup, quint32 posdown, quint32 posleft, quint32 posright);
    PkVector<quint16> IlluminatePixel();
    PkVector<quint16> IlluminatePixelFromNormalmap(qreal r, qreal g, qreal b);

    void setLightVector(QVector3D light_vector);

    ///Light sources to use (those disabled in the GUI are not present here)
    PkList<Illuminant> lightSources;

    ///Size of this stuff
    quint8 size;

    Illuminant fastLight;
    Illuminant fastLight2;

    bool diffuseLightIsEnabled;
    bool specularLightIsEnabled;

private:
    quint32 m_pixelArea;
};


#endif
