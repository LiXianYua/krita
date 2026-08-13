/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_animation_frame_cache_ui.h"

#include "kis_animation_frame_cache.h"
#include "opengl/kis_opengl_image_textures.h"

namespace
{
class KisOpenGLAnimationFrameCacheSource final : public KisAnimationFrameCacheSource
{
public:
    explicit KisOpenGLAnimationFrameCacheSource(KisOpenGLImageTexturesSP textures)
        : m_textures(std::move(textures))
    {
    }

    KisImageWSP image() const override
    {
        return m_textures->image();
    }

    KisOpenGLUpdateInfoBuilder &updateInfoBuilder() override
    {
        return m_textures->updateInfoBuilder();
    }

    KisOpenGLUpdateInfoSP fetchFrameData(const QRect &rect, KisImageSP image) override
    {
        return m_textures->updateCache(rect, image);
    }

    void uploadFrameData(KisOpenGLUpdateInfoSP info) override
    {
        m_textures->recalculateCache(info, false);
    }

private:
    KisOpenGLImageTexturesSP m_textures;
};
}

KisAnimationFrameCacheSP kisGetAnimationFrameCache(KisOpenGLImageTexturesSP textures)
{
    return KisAnimationFrameCache::getFrameCache(
        QSharedPointer<KisOpenGLAnimationFrameCacheSource>::create(std::move(textures)));
}
