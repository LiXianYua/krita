/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007, 2009 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2010 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef SVGGRADIENTHELPER_H
#define SVGGRADIENTHELPER_H

#include <KoFlakeCoordinateSystem.h>
#include <PkTransform.h>
#include <PkGradient.h>
#include <SvgMeshGradient.h>

class SvgGradientHelper
{
public:
    SvgGradientHelper();
    ~SvgGradientHelper();
    /// Copy constructor
    SvgGradientHelper(const SvgGradientHelper &other);

    /// Sets the gradient units type
    void setGradientUnits(KoFlake::CoordinateSystem units);
    /// Returns gradient units type
    KoFlake::CoordinateSystem gradientUnits() const;

    /// Sets the gradient
    void setGradient(PkGradient * g);
    /// Returns the gradient
    PkGradient * gradient() const;

    /// Sets the meshgradient
    void setMeshGradient(SvgMeshGradient* g);
    /// Returns the meshgradient
    PkScopedPointer<SvgMeshGradient>& meshgradient();

    // To distinguish between SvgMeshGradient and PkGradient
    bool isMeshGradient() const;

    /// Returns the gradient transformation
    PkTransform transform() const;
    /// Sets the gradient transformation
    void setTransform(const PkTransform &transform);

    /// Assignment operator
    SvgGradientHelper & operator = (const SvgGradientHelper & rhs);

    PkGradient * adjustedGradient(const PkRectF &bound) const;

    /// Converts a gradient from LogicalMode to ObjectBoundingMode 
    static PkGradient *convertGradient(const PkGradient * originalGradient, const PkTransform &userToRelativeTransform, const PkRectF &size);

    PkGradient::Spread spreadMode() const;
    void setSpreadMode(const PkGradient::Spread &spreadMode);

private:

    PkScopedPointer<PkGradient> m_gradient;
    PkScopedPointer<SvgMeshGradient> m_meshgradient;
    KoFlake::CoordinateSystem m_gradientUnits;
    PkTransform m_gradientTransform;
};

#endif // SVGGRADIENTHELPER_H
