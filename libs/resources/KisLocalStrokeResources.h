/*
 *  SPDX-FileCopyrightText: 2020 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISLOCALSTROKERESOURCES_H
#define KISLOCALSTROKERESOURCES_H

#include <KisResourcesInterface.h>

#include <PkList.h>

class KisLocalStrokeResourcesPrivate;


/**
 * @brief a KisResourcesInterface-like resources storage for preloaded resources
 *
 * KisLocalStrokeResources stores preloaded resources and dispatches them
 * to the consumers as a resources source.
 *
 * It is used by the strokes to avoid accessing global resource storage
 * from non-gui threads.
 */
class KRITARESOURCES_EXPORT KisLocalStrokeResources : public KisResourcesInterface
{
public:
    KisLocalStrokeResources();
    KisLocalStrokeResources(const PkList<KoResourceSP> &localResources);

    /**
     * Add a resource to this local resources storage
     */
    void addResource(KoResourceSP resource);

    /**
     * Remove a resource from this local resources storage
     */
    void removeResource(KoResourceSP resource);

    KisLocalStrokeResources* clone() const;

    /**
     * Return all the resources that are present in this local resources storage
     */
    PkList<KoResourceSP> resources() const;

protected:
    ResourceSourceAdapter* createSourceImpl(const PkString &type) const override;

private:
    // PIMPL 基类指针 d_ptr 指向的实际对象就是 KisLocalStrokeResourcesPrivate（见
    // KisLocalStrokeResources.cpp 的构造），reinterpret_cast 语义与 Qt 的
    // Q_DECLARE_PRIVATE 一致（后者展开也是 reinterpret_cast）；static_cast 到
    // 不完整类型（KisLocalStrokeResourcesPrivate 只有前置声明）是编译错误。
    KisLocalStrokeResourcesPrivate *d_func() const { return reinterpret_cast<KisLocalStrokeResourcesPrivate*>(d_ptr); }
    friend class KisLocalStrokeResourcesPrivate;
};

#endif // KISLOCALSTROKERESOURCES_H
