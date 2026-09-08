/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#ifndef KOMESHGRADIENTBACKGROUND_H
#define KOMESHGRADIENTBACKGROUND_H

#include "KoShapeBackground.h"
#include <PkSharedDataPointer.h>
#include "SvgMeshGradient.h"

class KRITAFLAKE_EXPORT KoMeshGradientBackground : public KoShapeBackground
{
public:
    KoMeshGradientBackground(const SvgMeshGradient *gradient, const PkTransform &matrix = PkTransform());
    ~KoMeshGradientBackground();

    // Work around MSVC inability to generate copy ops with PkSharedDataPointer.
    KoMeshGradientBackground(const KoMeshGradientBackground &);
    KoMeshGradientBackground& operator=(const KoMeshGradientBackground &);

    void paint(PkPainter &painter, const PkPainterPath &fillPath) const override;

    bool compareTo(const KoShapeBackground *other) const override;

    SvgMeshGradient* gradient();
    PkTransform transform();

private:
    class Private;
    PkSharedDataPointer<Private> d;
};


#endif // KOMESHGRADIENTBACKGROUND_H
