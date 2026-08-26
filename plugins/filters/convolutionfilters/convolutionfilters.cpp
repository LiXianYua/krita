/*
 * This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2004 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "convolutionfilters.h"

#include <stdlib.h>


#include <kis_convolution_kernel.h>
#include <kis_convolution_painter.h>

#include <filter/kis_filter_category_ids.h>
#include <filter/kis_filter_configuration.h>
#include <kis_selection.h>
#include <kis_paint_device.h>
#include <kis_processing_information.h>

#include <Eigen/Core>

namespace {
struct KritaConvolutionFiltersFilterRegistration
{
    KritaConvolutionFiltersFilterRegistration()
    {
        KisFilterRegistry::instance()->add(new KisSharpenFilter());
        KisFilterRegistry::instance()->add(new KisMeanRemovalFilter());
        KisFilterRegistry::instance()->add(new KisEmbossLaplascianFilter());
        KisFilterRegistry::instance()->add(new KisEmbossInAllDirectionsFilter());
        KisFilterRegistry::instance()->add(new KisEmbossHorizontalVerticalFilter());
        KisFilterRegistry::instance()->add(new KisEmbossVerticalFilter());
        KisFilterRegistry::instance()->add(new KisEmbossHorizontalFilter());
    }
};
} // namespace
static KritaConvolutionFiltersFilterRegistration s_kritaConvolutionFiltersFilterRegistration;


KisSharpenFilter::KisSharpenFilter()
        : KisConvolutionFilter(id(), FiltersCategoryEnhanceId, PkString("&Sharpen"))
{
    setSupportsPainting(true);
    setShowConfigurationWidget(false);

    Eigen::Matrix<qreal, Eigen::Dynamic, Eigen::Dynamic> kernelMatrix(3, 3);
    kernelMatrix <<  0, -2,   0,
                    -2,  11, -2,
                     0, -2,   0;

    m_matrix = KisConvolutionKernel::fromMatrix(kernelMatrix, 0, 3);
}

KisMeanRemovalFilter::KisMeanRemovalFilter()
        : KisConvolutionFilter(id(), FiltersCategoryEnhanceId, PkString("&Mean Removal"))
{
    setSupportsPainting(false);
    setShowConfigurationWidget(false);

    Eigen::Matrix<qreal, Eigen::Dynamic, Eigen::Dynamic> kernelMatrix(3, 3);
    kernelMatrix << -1, -1, -1,
                    -1,  9, -1,
                    -1, -1, -1;

    m_matrix = KisConvolutionKernel::fromMatrix(kernelMatrix, 0, 1);
}

KisEmbossLaplascianFilter::KisEmbossLaplascianFilter()
        : KisConvolutionFilter(id(), FiltersCategoryEmbossId, PkString("Emboss (Laplacian)"))
{
    setSupportsPainting(false);
    setShowConfigurationWidget(false);

    Eigen::Matrix<qreal, Eigen::Dynamic, Eigen::Dynamic> kernelMatrix(3, 3);
    kernelMatrix << -1, 0, -1,
                     0, 4,  0,
                    -1, 0, -1;

    m_matrix = KisConvolutionKernel::fromMatrix(kernelMatrix, 0.5, 1);
    setIgnoreAlpha(true);
}

KisEmbossInAllDirectionsFilter::KisEmbossInAllDirectionsFilter()
        : KisConvolutionFilter(id(), FiltersCategoryEmbossId, PkString("Emboss in All Directions"))
{
    setSupportsPainting(false);
    setShowConfigurationWidget(false);

    Eigen::Matrix<qreal, Eigen::Dynamic, Eigen::Dynamic> kernelMatrix(3, 3);
    kernelMatrix << -1, -1, -1,
                    -1,  8, -1,
                    -1, -1, -1;

    m_matrix = KisConvolutionKernel::fromMatrix(kernelMatrix, 0.5, 1);
    setIgnoreAlpha(true);
}

KisEmbossHorizontalVerticalFilter::KisEmbossHorizontalVerticalFilter()
        : KisConvolutionFilter(id(), FiltersCategoryEmbossId, PkString("Emboss Horizontal && Vertical"))
{
    setSupportsPainting(false);
    setShowConfigurationWidget(false);

    Eigen::Matrix<qreal, Eigen::Dynamic, Eigen::Dynamic> kernelMatrix(3, 3);
    kernelMatrix <<  0, -1,  0,
                    -1,  4, -1,
                     0, -1,  0;

    m_matrix = KisConvolutionKernel::fromMatrix(kernelMatrix, 0.5, 1);
    setIgnoreAlpha(true);
}

KisEmbossVerticalFilter::KisEmbossVerticalFilter()
        : KisConvolutionFilter(id(), FiltersCategoryEmbossId, PkString("Emboss Vertical Only"))
{
    setSupportsPainting(false);
    setShowConfigurationWidget(false);

    Eigen::Matrix<qreal, Eigen::Dynamic, Eigen::Dynamic> kernelMatrix(3, 3);
    kernelMatrix << 0, -1, 0,
                    0,  2, 0,
                    0, -1, 0;

    m_matrix = KisConvolutionKernel::fromMatrix(kernelMatrix, 0.5, 1);
    setIgnoreAlpha(true);
}

KisEmbossHorizontalFilter::KisEmbossHorizontalFilter() :
        KisConvolutionFilter(id(), FiltersCategoryEmbossId, PkString("Emboss Horizontal Only"))
{
    setSupportsPainting(false);
    setShowConfigurationWidget(false);

    Eigen::Matrix<qreal, Eigen::Dynamic, Eigen::Dynamic> kernelMatrix(3, 3);
    kernelMatrix <<  0, 0,  0,
                    -1, 2, -1,
                     0, 0,  0;

    m_matrix = KisConvolutionKernel::fromMatrix(kernelMatrix, 0.5, 1);
    setIgnoreAlpha(true);
}

KisEmbossDiagonalFilter::KisEmbossDiagonalFilter()
        : KisConvolutionFilter(id(), FiltersCategoryEdgeDetectionId, PkString("Top Edge Detection"))
{
    setSupportsPainting(false);
    setShowConfigurationWidget(false);

    Eigen::Matrix<qreal, Eigen::Dynamic, Eigen::Dynamic> kernelMatrix(3, 3);
    kernelMatrix << -1, 0, -1,
                     0, 4,  0,
                    -1, 0, -1;

    m_matrix = KisConvolutionKernel::fromMatrix(kernelMatrix, 0.5, 1);
    setIgnoreAlpha(true);
}
