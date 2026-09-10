/*
 * SPDX-FileCopyrightText: 2006-2010 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2010 Halla Rempt <halla@valdyas.org>
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KOTOOLBASE_P_H
#define KOTOOLBASE_P_H

#include "KoDocumentResourceManager.h"
#include "KoCanvasResourceProvider.h"
#include "KoCanvasBase.h"
#include "KoShapeController.h"
#include <PkHash.h>
#include <PkPointer.h>
// connectSignals() 直用 Q_ASSERT_X，故此头自己包 compat/QtGlobal（不再靠
// 已删掉的 <QCursor> 间接带入）。两个桶都成立：native 桶解析到
// pk/global/compat/QtGlobal，qt 桶解析到真 Qt 头。
#include <QtGlobal>
#include <string.h> // for the qt version check

class QAction;
class KoToolBase;
class KoToolFactoryBase;

class KoToolBasePrivate
{
public:
    KoToolBasePrivate(KoToolBase *qq, KoCanvasBase *canvas_)
        : q(qq),
        canvas(canvas_),
        isInTextMode(false),
        isActivated(false)
    {
    }

    virtual ~KoToolBasePrivate() = default;

    void connectSignals()
    {
        if (canvas) { // in the case of KoToolManagers dummy tool it can be zero :(
            KoCanvasResourceProvider * crp = canvas->resourceManager();
            Q_ASSERT_X(crp, "KoToolBase::KoToolBase", "No Canvas KoResourceManager");
            if (crp) {
                const PkPointer<KoToolBase> guard(q);
                PkObject::connect(crp, &KoCanvasResourceProvider::canvasResourceChanged, crp,
                                 [guard](int key, const PkVariant &value) {
                    if (guard) guard->canvasResourceChanged(key, value);
                });
            }

            KoDocumentResourceManager *scrm = canvas->shapeController()->resourceManager();
            if (scrm) {
                const PkPointer<KoToolBase> guard(q);
                PkObject::connect(scrm, &KoDocumentResourceManager::resourceChanged, scrm,
                                 [guard](int key, const PkVariant &value) {
                    if (guard) guard->documentResourceChanged(key, value);
                });
            }
        }
    }

    struct ToolCanvasResources {
        PkHash<int, KoAbstractCanvasResourceInterfaceSP> abstractResources;
        PkHash<int, KoDerivedResourceConverterSP> converters;
    };

    KisCanvasCursorToken currentCursorToken;
    KoToolBase *q;
    KoToolFactoryBase *factory {0};
    KoCanvasBase *canvas; ///< the canvas interface this tool will work for.
    bool isInTextMode;
    bool maskSyntheticEvents{false}; ///< Whether this tool masks synthetic events
    bool isActivated;
    PkRectF lastDecorationsRect;
    bool isOpacityPresetMode{false}; ///< Whether the opacity is preset or tool
    ToolCanvasResources toolCanvasResources;

};

#endif
