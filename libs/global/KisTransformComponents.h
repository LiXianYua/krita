/*
 *  SPDX-FileCopyrightText: 2025 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkFlags.h>
#include <type_traits>

#include <kritaglobal_export.h>

namespace KisAlgebra2D
{

enum KisTransformComponent
{
    Translate = 0x1,
    Scale = 0x2,
    Rotate = 0x4,
    Shear = 0x8,
    Project = 0x10
};

Q_DECLARE_FLAGS(KisTransformComponents, KisTransformComponent);

KisTransformComponents KRITAGLOBAL_EXPORT makeFullTransformComponents();
KisTransformComponents KRITAGLOBAL_EXPORT componentsForTransform(const PkTransform &t);
KisTransformComponents KRITAGLOBAL_EXPORT compareTransformComponents(const PkTransform &lhs, const PkTransform &rhs);
}

Q_DECLARE_METATYPE(KisAlgebra2D::KisTransformComponents)
Q_DECLARE_OPERATORS_FOR_FLAGS(KisAlgebra2D::KisTransformComponents)

// we don't use Q_FLAGS's autogeneration of PkDebug here because we
// want to avoid adding Q_NAMESPACE to KisAlgebra2D
PkDebug KRITAGLOBAL_EXPORT operator<<(PkDebug dbg, KisAlgebra2D::KisTransformComponent component);
PkDebug KRITAGLOBAL_EXPORT operator<<(PkDebug dbg, KisAlgebra2D::KisTransformComponents components);
