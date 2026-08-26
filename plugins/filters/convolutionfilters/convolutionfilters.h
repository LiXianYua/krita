/*
 * This file is part of Krita
 *
 * SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef CONVOLUTIONFILTERS_H
#define CONVOLUTIONFILTERS_H

#include "kis_convolution_filter.h"

class KisSharpenFilter : public KisConvolutionFilter
{
public:
    KisSharpenFilter();
public:
    static inline KoID id() {
        return KoID("sharpen", PkString("Sharpen"));
    }

};

class KisMeanRemovalFilter : public KisConvolutionFilter
{
public:
    KisMeanRemovalFilter();
public:
    static inline KoID id() {
        return KoID("mean removal", PkString("Mean Removal"));
    }
};

class KisEmbossLaplascianFilter : public KisConvolutionFilter
{
public:
    KisEmbossLaplascianFilter();
public:
    static inline KoID id() {
        return KoID("emboss laplascian", PkString("Emboss (Laplacian)"));
    }
};

class KisEmbossInAllDirectionsFilter : public KisConvolutionFilter
{
public:
    KisEmbossInAllDirectionsFilter();
public:
    static inline KoID id() {
        return KoID("emboss all directions", PkString("Emboss in All Directions"));
    }
};

class KisEmbossHorizontalVerticalFilter : public KisConvolutionFilter
{
public:
    KisEmbossHorizontalVerticalFilter();
public:
    static inline KoID id() {
        return KoID("emboss horizontal and vertical", PkString("Emboss Horizontal & Vertical"));
    }
};

class KisEmbossVerticalFilter : public KisConvolutionFilter
{
public:
    KisEmbossVerticalFilter();
public:
    static inline KoID id() {
        return KoID("emboss vertical only", PkString("Emboss Vertical Only"));
    }
};

class KisEmbossHorizontalFilter : public KisConvolutionFilter
{
public:
    KisEmbossHorizontalFilter();
public:
    static inline KoID id() {
        return KoID("emboss horizontal only", PkString("Emboss Horizontal Only"));
    }
};

class KisEmbossDiagonalFilter : public KisConvolutionFilter
{
public:
    KisEmbossDiagonalFilter();
public:
    static inline KoID id() {
        return KoID("emboss diagonal", PkString("Emboss Diagonal"));
    }
};


#endif
