/* SPDX-License-Identifier: GPL-2.0-or-later */

#include "ImageShapeState.h"

#include <SvgUtil.h>

ImageShapeState::ImageShapeState() = default;

ImageShapeState::ImageShapeState(const ImageShapeState &other)
    : image(other.image)
    , ratioParser(other.ratioParser
          ? std::make_unique<SvgUtil::PreserveAspectRatioParser>(*other.ratioParser)
          : nullptr)
    , viewBoxTransform(other.viewBoxTransform)
{
}

ImageShapeState::ImageShapeState(ImageShapeState &&other) noexcept = default;

ImageShapeState &ImageShapeState::operator=(const ImageShapeState &other)
{
    if (this != &other) {
        ImageShapeState copy(other);
        *this = std::move(copy);
    }
    return *this;
}

ImageShapeState &ImageShapeState::operator=(ImageShapeState &&other) noexcept = default;
ImageShapeState::~ImageShapeState() = default;

ImageShapeStateHolder::ImageShapeStateHolder()
    : m_state(std::make_shared<ImageShapeState>())
{
}

const ImageShapeState &ImageShapeStateHolder::read() const
{
    return *m_state;
}

ImageShapeState &ImageShapeStateHolder::write()
{
    if (!m_state.unique()) m_state = std::make_shared<ImageShapeState>(*m_state);
    return *m_state;
}
