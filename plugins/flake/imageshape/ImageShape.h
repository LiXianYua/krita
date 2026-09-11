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
#include <PkPainter.h>

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

    // KoShape::paint 是纯虚（libs/flake/KoShape.h:161 `= 0`）。本类派生
    // KoShape+SvgShape，工厂要 new 它，所以必须给出实现——否则类仍是抽象类，
    // ImageShapeFactory 的两个 new 点编不过。渲染语义照 flake 内的同胞实现
    // libs/flake/shapes/ImageShape.cpp:61-68（同一份 viewBoxTransform + image）。
    void paint(PkPainter &painter) const override;

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
