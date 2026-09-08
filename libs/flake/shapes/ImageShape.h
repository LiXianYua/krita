/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IMAGESHAPE_H
#define IMAGESHAPE_H

#include <PkSharedDataPointer.h>

#include "KoShape.h"
#include <SvgShape.h>

#define ImageShapeId "ImageShape"


class KRITAFLAKE_EXPORT ImageShape : public KoShape, public SvgShape
{
public:
    ImageShape();
    ~ImageShape() override;

    KoShape *cloneShape() const override;

    void paint(PkPainter &painter) const override;

    void setSize(const PkSizeF &size) override;

    bool saveSvg(SvgSavingContext &context) override;
    bool loadSvg(const PkXmlElement &element, SvgLoadingContext &context) override;

    void setImage(const PkImage &img);
    PkImage image() const;

    void setViewBoxTransform(const PkTransform &tf);
    PkTransform viewBoxTransform() const;

private:
    ImageShape(const ImageShape &rhs);

private:
    struct Private;
    PkSharedDataPointer<Private> m_d;
};

#endif // IMAGESHAPE_H
