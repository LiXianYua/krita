/*
 *  SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

// ===========================================================================
// [GAP] kis_paintop_registry.cc 阻塞登记（S-06 Task 4）
//
// 本文件不进薄壳，仅剥可机械映射类型（源文件 Q* 已归零）。阻塞原因：
//   * paintOp() 以值传 KisNodeSP / KisImageSP（KisSharedPtr 的 ref/deref 需要
//     完整类型定义），所以必须 include kis_layer.h / kis_image.h
//   * kis_layer.h → kis_psd_layer_style.h → psd.h（未剥），未剥的
//     KisPSDLayerStyle 仍用 Qt 列表容器覆盖 KoResource 已被剥成 PkVector 的虚函数
//     （linkedResources/sideLoadedResources/requiredCanvasResources），
//     协变返回类型不匹配 —— 跨任务类型断裂
// 关闭条件：layerstyles 任务把 kis_psd_layer_style.h 的返回类型剥成
// PkVector 后，本文件可加入薄壳 SHELL_SOURCES。当前状态：签名已与剥离后的
// 头文件对齐（KisNodeSP/KisImageSP 由 kis_types.h 提供），Qt 仅经未剥依赖头
// 传递进入，不参与薄壳构建。
// ===========================================================================

#include "kis_paintop_registry.h"

#include <KoGenericRegistry.h>
#include <KoColorSpace.h>
#include <KoColorSpaceRegistry.h>
#include <KoCompositeOp.h>
#include <KoID.h>


#include "kis_paint_device.h"
#include "kis_painter.h"
#include "kis_debug.h"
#include "kis_layer.h"
#include "kis_image.h"

KisPaintOpRegistry::KisPaintOpRegistry()
{
}

KisPaintOpRegistry::~KisPaintOpRegistry()
{
    for (const PkString & id : keys()) {
        delete get(id);
    }
    dbgRegistry << "Deleting KisPaintOpRegistry";
}

KisPaintOpRegistry* KisPaintOpRegistry::instance()
{
    static KisPaintOpRegistry s_registryInstance;
    return &s_registryInstance;
}

#ifdef HAVE_THREADED_TEXT_RENDERING_WORKAROUND
void KisPaintOpRegistry::preinitializePaintOpIfNeeded(const KisPaintOpPresetSP preset)
{
    if (!preset) return;

    KisPaintOpFactory *f = value(preset->paintOp().id());
    f->preinitializePaintOpIfNeeded(preset->settings());
}
#endif /* HAVE_THREADED_TEXT_RENDERING_WORKAROUND */

KisPaintOp * KisPaintOpRegistry::paintOp(const PkString & id, const KisPaintOpSettingsSP settings, KisPainter * painter, KisNodeSP node, KisImageSP image) const
{
    if (painter == 0) {
        warnKrita << " KisPaintOpRegistry::paintOp painter is null";
        return 0;
    }

    Q_ASSERT(settings);

    KisPaintOpFactory* f = value(id);
    if (f) {
        KisPaintOp * op = f->createOp(settings, painter, node, image);
        if (op) {
            return op;
        }
    }
    warnKrita << "Could not create paintop for factory" << id << "with settings" << settings;
    return 0;
}

KisPaintOp * KisPaintOpRegistry::paintOp(const KisPaintOpPresetSP preset, KisPainter * painter, KisNodeSP node, KisImageSP image) const
{
    if (!preset) return 0;
    if (!painter) return 0;
    return paintOp(preset->paintOp().id(), preset->settings(), painter, node, image);
}

KisInterstrokeDataFactory *KisPaintOpRegistry::createInterstrokeDataFactory(KisPaintOpPresetSP preset) const
{
    if (preset) {
        KisPaintOpFactory* f = value(preset->paintOp().id());
        if (f) {
            return f->createInterstrokeDataFactory(preset->settings(), preset->resourcesInterface());
        }
    }

    return 0;
}

KisPaintOpSettingsSP KisPaintOpRegistry::createSettings(const KoID& id, KisResourcesInterfaceSP resourcesInterface) const
{
    KisPaintOpFactory *f = value(id.id());
    Q_ASSERT(f);
    if (f) {
        KisPaintOpSettingsSP settings = f->createSettings(resourcesInterface);
        settings->setProperty("paintop", id.id());
        return settings;
    }
    return 0;
}

KisPaintOpPresetSP KisPaintOpRegistry::defaultPreset(const KoID& id, KisResourcesInterfaceSP resourcesInterface) const
{
    KisPaintOpSettingsSP s = createSettings(id, resourcesInterface);
    if (s.isNull()) {
        return KisPaintOpPresetSP();
    }

    KisPaintOpPresetSP preset(new KisPaintOpPreset());
    preset->setName(PkString("default"));

    preset->setSettings(s);
    preset->setPaintOp(id);
    Q_ASSERT(!preset->paintOp().id().isEmpty());
    preset->setValid(true);
    return preset;
}

PkList<KoID> KisPaintOpRegistry::listKeys() const
{
    PkList<KoID> answer;
    for (const PkString & key : keys()) {
        answer.append(KoID(key, get(key)->name()));
    }

    return answer;
}

