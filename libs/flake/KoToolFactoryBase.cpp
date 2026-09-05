/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoToolFactoryBase.h"

#include "KoToolBase.h"

#include <KoToolManager.h>

#include <QKeySequence>
#include <QAction>
#include <QDebug>

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
    QKeySequence shortcut;
};


KoToolFactoryBase::KoToolFactoryBase(const PkString &id)
    : d(new Private(id))
{
}

KoToolFactoryBase::~KoToolFactoryBase()
{
    delete d;
}

PkList<QAction *> KoToolFactoryBase::createActions(QObject *actionCollection)
{
    PkList<QAction *> toolActions;

    PkList<QAction*> actions = createActionsImpl();
    QAction *action = new QAction(this);
    action->setObjectName(id());
    if (actionCollection) {
        action->setParent(actionCollection);
    }
    connect(action, &QAction::triggered, this, &KoToolFactoryBase::activateTool);
    //qDebug() << action << action->shortcut();


    Q_FOREACH(QAction *action, actions) {
        if (action->objectName().isEmpty()) {
            qWarning() << "Tool" << id() << "tries to add an action without a name";
            continue;
        }
        QAction *existingAction = actionCollection ? actionCollection->findChild<QAction *>(action->objectName()) : 0;
        if (existingAction) {
            delete action;
            action = existingAction;
        }

        PkStringList tools;
        if (action->property("tool_action").isValid()) {
            tools = action->property("tool_action").toStringList();
        }
        tools << id();
        action->setProperty("tool_action", tools);
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

QKeySequence KoToolFactoryBase::shortcut() const
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

void KoToolFactoryBase::setShortcut(const QKeySequence &shortcut)
{
    d->shortcut = shortcut;
}

PkList<QAction *> KoToolFactoryBase::createActionsImpl()
{
    return PkList<QAction *>();
}

void KoToolFactoryBase::activateTool()
{
    KoToolManager::instance()->switchToolRequested(sender()->objectName());
}

