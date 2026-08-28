/* SPDX-License-Identifier: GPL-2.0-or-later */

#pragma once

#include <PkImage.h>
#include <PkTransform.h>
#include <SvgUtil.h>

#include <memory>

struct ImageShapeState
{
    ImageShapeState();
    ImageShapeState(const ImageShapeState &other);
    ImageShapeState(ImageShapeState &&other) noexcept;
    ImageShapeState &operator=(const ImageShapeState &other);
    ImageShapeState &operator=(ImageShapeState &&other) noexcept;
    ~ImageShapeState();

    PkImage image;
    std::unique_ptr<SvgUtil::PreserveAspectRatioParser> ratioParser;
    PkTransform viewBoxTransform;
};

class ImageShapeStateHolder
{
public:
    ImageShapeStateHolder();
    ImageShapeStateHolder(const ImageShapeStateHolder &) = default;
    ImageShapeStateHolder(ImageShapeStateHolder &&) noexcept = default;
    ImageShapeStateHolder &operator=(const ImageShapeStateHolder &) = default;
    ImageShapeStateHolder &operator=(ImageShapeStateHolder &&) noexcept = default;

    const ImageShapeState &read() const;
    ImageShapeState &write();

private:
    std::shared_ptr<ImageShapeState> m_state;
};
