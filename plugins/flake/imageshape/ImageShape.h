/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef IMAGESHAPE_H
#define IMAGESHAPE_H

#include <PkImage.h>
#include <PkTransform.h>
#include <PkXmlElement.h>

#include "ImageShapeState.h"
#include "KoShape.h"
#include <SvgShape.h>

#define ImageShapeId "ImageShape"


class ImageShape : public KoShape, public SvgShape
{
public:
    ImageShape();
    ~ImageShape() override;
    ImageShape &operator=(const ImageShape &) = delete;

    KoShape *cloneShape() const override;

    // The current Qt painter hook remains transitional and is owned by M5.
    // Do not invent a replacement renderer contract in this value-side class.
    void setSize(const PkSizeF &size);

    bool saveSvg(SvgSavingContext &context) override;
    bool loadSvg(const PkXmlElement &element, SvgLoadingContext &context) override;

    void setImage(const PkImage &img);
    PkImage image() const;

    void setViewBoxTransform(const PkTransform &tf);
    PkTransform viewBoxTransform() const;

private:
    ImageShape(const ImageShape &rhs);
    ImageShapeStateHolder m_state;
};

#endif // IMAGESHAPE_H
