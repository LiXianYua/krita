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

#include <memory>

#include "KoShape.h"
#include <SvgShape.h>

#define ImageShapeId "ImageShape"


class ImageShape : public KoShape, public SvgShape
{
public:
    ImageShape();
    ~ImageShape() override;

    KoShape *cloneShape() const override;

    // S-09/M5 GAP: the drawing hook remains callable, but the kernel has no
    // painter backend yet.  The opaque context is deliberately unused.
    void paint(void *paintContext) const override;

    void setSize(const PkSizeF &size) override;

    bool saveSvg(SvgSavingContext &context) override;
    bool loadSvg(const PkXmlElement &element, SvgLoadingContext &context) override;

    void setImage(const PkImage &img);
    PkImage image() const;

    void setViewBoxTransform(const PkTransform &tf);
    PkTransform viewBoxTransform() const;

private:
    ImageShape(const ImageShape &rhs);
    void detach();

private:
    struct Private;
    std::shared_ptr<Private> m_d;
};

#endif // IMAGESHAPE_H
