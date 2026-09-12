/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoToolFactoryBase.h"

#include "KoToolBase.h"

#include <KoToolManager.h>

#include <QAction>
#include <QDebug>
#include <klocalizedstring.h>

#include <PkFlakeBridge.h>

namespace {
QString translateHostActionText(const char *text)
{
    if (!text || !*text) {
        return QString();
    }
    // Keep these literals in the host boundary so xgettext can retain the
    // contextless message ids used by the QAction runtime path.
    if (qstrcmp(text, "Increase Brush Size") == 0) {
        return i18n("Increase Brush Size");
    }
    if (qstrcmp(text, "Decrease Brush Size") == 0) {
        return i18n("Decrease Brush Size");
    }
    if (qstrcmp(text, "Rotate brush tip clockwise") == 0) {
        return i18n("Rotate brush tip clockwise");
    }
    if (qstrcmp(text, "Rotate brush tip clockwise (precise)") == 0) {
        return i18n("Rotate brush tip clockwise (precise)");
    }
    if (qstrcmp(text, "Rotate brush tip counter-clockwise") == 0) {
        return i18n("Rotate brush tip counter-clockwise");
    }
    if (qstrcmp(text, "Rotate brush tip counter-clockwise (precise)") == 0) {
        return i18n("Rotate brush tip counter-clockwise (precise)");
    }
    return i18n(text);
}
}

class Q_DECL_HIDDEN KoToolFactoryBase::Private
{
public:
    Private(const PkString &i)
        : priority(100),
          id(i)
    {
    }
    int priority;
    PkString section;
    PkString tooltip;
    PkString activationId;
    PkString iconName;
    const PkString id;
    PkKeySequence shortcut;
    QObject actionOwner;
};


KoToolFactoryBase::KoToolFactoryBase(const PkString &id)
    : d(new Private(id))
{
}

KoToolFactoryBase::~KoToolFactoryBase()
{
    delete d;
}

PkList<QAction *> KoToolFactoryBase::createActions(PkObject *actionCollection)
{
    PkList<QAction *> toolActions;

    const PkList<KisHostActionSpec> actionSpecs = createActionsImpl();

    QAction *toolActivationAction = new QAction(actionCollection);
    toolActivationAction->setObjectName(toQString(id()));
    if (actionCollection) {
        toolActivationAction->setParent(actionCollection);
    }
    QObject::connect(toolActivationAction, &QAction::triggered, toolActivationAction,
                     [toolId = id()] { KoToolManager::instance()->switchToolRequested(toolId); });
    //qDebug() << action << action->shortcut();


    Q_FOREACH(const KisHostActionSpec &spec, actionSpecs) {
        if (spec.objectName.isEmpty()) {
            qWarning() << "Tool" << id() << "tries to add an action without a name";
            continue;
        }

        // 物化边界：把桶无关的 spec 变成真的 action 对象。native 桶里 QAction 是
        // noqt-compat 垫片（PK_CAT_ 拼装出同名类），qt 桶里是真 QAction。候选先挂在
        // d->actionOwner 上——它不在任何 action collection 里，重复项查找便不会顺着
        // 父链找到候选、把它自己删掉。翻译（translateHostActionText）也只在这里发生。
        QAction *action = new QAction(translateHostActionText(spec.text), &d->actionOwner);
        action->setObjectName(toQString(spec.objectName));
        if (spec.shortcut != static_cast<Pk::Key>(0)) {
            action->setShortcut(static_cast<int>(spec.shortcut));
        }

        QAction *existingAction = actionCollection ? actionCollection->findChild<QAction *>(action->objectName()) : 0;
        if (existingAction) {
            delete action;
            action = existingAction;
        }

        PkStringList tools;
        if (action->property("tool_action").isValid()) {
            tools = toPkStringList(action->property("tool_action").toStringList());
        }
        tools << id();
        action->setProperty("tool_action", toQStringList(tools));
        if (!existingAction && actionCollection) {
            action->setParent(actionCollection);
        }
        toolActions << action;
    }

    // Enable this to easily generate action files for tools
 #if 0
    if (toolActions.size() > 0) {

        PkXmlDocument doc;
        PkXmlElement e = doc.createElement("Actions");
        e.setAttribute("name", id);
        e.setAttribute("version", "2");
        doc.appendChild(e);

        Q_FOREACH (QAction *action, toolActions) {
            PkXmlElement a = doc.createElement("Action");
            a.setAttribute("name", action->objectName());

            // But seriously, XML is the worst format ever designed
            auto addElement = [&](PkString title, PkString content) {
                PkXmlElement newNode = doc.createElement(title);
                PkXmlText    newText = doc.createTextNode(content);
                newNode.appendChild(newText);
                a.appendChild(newNode);
            };

            addElement("icon", action->icon().name());
            addElement("text", action->text());
            addElement("whatsThis" , action->whatsThis());
            addElement("toolTip" , action->toolTip());
            addElement("iconText" , action->iconText());
            addElement("shortcut" , action->shortcut().toString());
            addElement("isCheckable" , PkString((action->isChecked() ? "true" : "false")));
            addElement("statusTip", action->statusTip());
            e.appendChild(a);
        }
        PkFileStream f(id()z + ".action");
        f.open(PkFileStream::WriteOnly);
        f.write(doc.toString().toUtf8());
        f.close();

    }

    else {
        debugFlake << "Tool" << id() << "has no actions";
    }
#endif

//    qDebug() << "Generated actions for tool factory" << id();
//    Q_FOREACH(QAction *action, toolActions) {
//        qDebug() << "\taction:" << action->objectName() << "shortcut" << action->shortcuts() << "tools" << action->property("tool_action").toStringList();
//    }
    return toolActions;
}

PkString KoToolFactoryBase::id() const
{
    return d->id;
}

int KoToolFactoryBase::priority() const
{
    return d->priority;
}

PkString KoToolFactoryBase::section() const
{
    return d->section;
}

PkString KoToolFactoryBase::toolTip() const
{
    return d->tooltip;
}

PkString KoToolFactoryBase::iconName() const
{
    return d->iconName;
}

PkString KoToolFactoryBase::activationShapeId() const
{
    return d->activationId;
}

PkKeySequence KoToolFactoryBase::shortcut() const
{
    return d->shortcut;
}

void KoToolFactoryBase::setActivationShapeId(const PkString &activationShapeId)
{
    d->activationId = activationShapeId;
}

void KoToolFactoryBase::setToolTip(const PkString & tooltip)
{
    d->tooltip = tooltip;
}

void KoToolFactoryBase::setSection(const PkString & section)
{
    d->section = section;
}

void KoToolFactoryBase::setIconName(const char *iconName)
{
    d->iconName = toPkString(QLatin1String(iconName));
}

void KoToolFactoryBase::setIconName(const PkString &iconName)
{
    d->iconName = iconName;
}

void KoToolFactoryBase::setPriority(int newPriority)
{
    d->priority = newPriority;
}

void KoToolFactoryBase::setShortcut(const PkKeySequence &shortcut)
{
    d->shortcut = shortcut;
}

PkList<KisHostActionSpec> KoToolFactoryBase::createActionsImpl()
{
    return PkList<KisHostActionSpec>();
}

KisHostActionSpec KoToolFactoryBase::createHostAction(const char *text,
                                                      const PkString &objectName,
                                                      Pk::Key shortcut)
{
    KisHostActionSpec spec;
    spec.text = text;
    spec.objectName = objectName;
    spec.shortcut = shortcut;
    return spec;
}
