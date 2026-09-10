/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2010 Boudewijn Rempt <boud@valdyas.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoCanvasController.h"
#include "KoToolManager.h"
#include "KoToolManagerShortcuts_p.h"

#include <PkFlakeBridge.h>
#include <PkSize.h>
#include <PkPoint.h>

#include <QAction>
#include <QDebug>

namespace {

// 宿主快捷键载荷 → 桶无关的 encoded chord：丢掉空 chord，逐和弦取 int。
// 这段逻辑原本在 KoToolManagerShortcuts::fromHostAction()（管理器侧）；本轮移到
// 宿主实现者这里，管理器只剩「比较编码」这一件事，不再需要命名宿主的快捷键类型。
PkList<KoToolManagerShortcuts::EncodedShortcut> encodeHostActionShortcuts(const QAction &action)
{
    PkList<KoToolManagerShortcuts::EncodedShortcut> result;
    for (const auto &hostShortcut : action.shortcuts()) {
        if (hostShortcut.toString().isEmpty()) {
            continue;
        }

        KoToolManagerShortcuts::EncodedShortcut chords;
        chords.reserve(static_cast<std::size_t>(hostShortcut.count()));
        for (int i = 0; i < hostShortcut.count(); ++i) {
            chords.push_back(hostShortcut[i]);
        }
        result.append(std::move(chords));
    }
    return result;
}

}

class Q_DECL_HIDDEN KoCanvasController::Private
{
public:
    Private()
        : preferredCenterFractionX(0.5)
        , preferredCenterFractionY(0.5)
        , actionCollection(0)
    {
    }

    PkSizeF documentSize;
    PkPoint documentOffset;
    qreal preferredCenterFractionX;
    qreal preferredCenterFractionY;
    QObject *actionCollection;
};

KoCanvasController::KoCanvasController(QObject *actionCollection)
    : d(new Private())
{
    proxyObject = new KoCanvasControllerProxyObject(this);
    d->actionCollection = actionCollection;
}

KoCanvasController::~KoCanvasController()
{
    KoToolManager::instance()->removeCanvasController(this);
    delete d;
    delete proxyObject;
}

KoCanvasBase* KoCanvasController::canvas() const
{
    return 0;
}

KoCanvasControllerProxyObject::KoCanvasControllerProxyObject(KoCanvasController *controller, PkObject *parent)
    : PkObject(parent)
    , m_canvasController(controller)
{
}

QObject *KoCanvasController::actionCollection() const
{
    return d->actionCollection;
}

PkList<KisHostActionIdentity> KoCanvasController::hostActions() const
{
    PkList<KisHostActionIdentity> identities;

    if (!d->actionCollection) {
        return identities;
    }

    // 遍历面与旧 KoToolManager::activateToolActions() 完全一致：整个动作集合的全部
    // QAction 子项（含没有 tool_action 属性的那些 —— 它们在管理器侧归入 globalActions）。
    const PkList<QAction *> hostActions = d->actionCollection->findChildren<QAction *>();

    for (QAction *action : hostActions) {
        KisHostActionIdentity identity;
        identity.objectName = toPkString(action->objectName());
        identity.carriesToolAction = action->property("tool_action").isValid();
        if (identity.carriesToolAction) {
            identity.toolIds = toPkStringList(action->property("tool_action").toStringList());
        }
        identity.alwaysEnabled = action->property("always_enabled").toBool();
        identity.shortcutChords = encodeHostActionShortcuts(*action);
        identities.append(std::move(identity));
    }

    return identities;
}

void KoCanvasController::setHostActionEnabled(const PkString &objectName, bool enabled)
{
    if (!d->actionCollection) {
        return;
    }

    // 旧实现直接解引用 findChild() 的结果（名字对不上就崩）；这里补上判空，
    // 对名字存在的路径行为逐字相同。
    QAction *action = d->actionCollection->findChild<QAction *>(toQString(objectName));
    if (action) {
        action->setEnabled(enabled);
    }
}
