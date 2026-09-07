/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Boudewijn Rempt (boud@valdyas.org)
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2008 C. Boemann <cbo@boemann.dk>
 * SPDX-FileCopyrightText: 2008 Thorsten Zachmann <zachmann@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include <QtCore/QtCore>
#include <PkFlakeBridge.h>
#include "KoShapeFactoryBase.h"

#include <QDebug>

#include "KoDocumentResourceManager.h"
#include "KoDeferredShapeFactoryBase.h"
#include "KoShape.h"
#include "KoShapeLoadingContext.h"

#include <KoProperties.h>

#include <PkMutex.h>
#include <PkMutex.h>
#include <PkPointer.h>


#include <FlakeDebug.h>

class Q_DECL_HIDDEN KoShapeFactoryBase::Private
{
public:
    Private(const PkString &_id, const PkString &_name, const PkString &_deferredPluginName)
        : deferredFactory(0),
          deferredPluginName(_deferredPluginName),
          id(_id),
          name(_name),
          loadingPriority(0),
          hidden(false)
    {
    }

    ~Private() {
        Q_FOREACH (const KoShapeTemplate & t, templates)
            delete t.properties;
        templates.clear();
    }

    KoDeferredShapeFactoryBase *deferredFactory;
    PkMutex pluginLoadingMutex;
    PkString deferredPluginName;
    PkList<KoShapeTemplate> templates;
    const PkString id;
    const PkString name;
    PkString family;
    PkString tooltip;
    PkString iconName;
    int loadingPriority;
    PkList<std::pair<PkString, PkStringList> > xmlElements; // xml name space -> xml element names
    bool hidden;
    PkList<PkPointer<KoDocumentResourceManager> > resourceManagers;
};


KoShapeFactoryBase::KoShapeFactoryBase(const PkString &id, const PkString &name, const PkString &deferredPluginName)
    : d(new Private(id, name, deferredPluginName))
{
}

KoShapeFactoryBase::~KoShapeFactoryBase()
{
    delete d;
}

PkString KoShapeFactoryBase::toolTip() const
{
    return d->tooltip;
}

PkString KoShapeFactoryBase::iconName() const
{
    return d->iconName;
}

PkString KoShapeFactoryBase::name() const
{
    return d->name;
}

PkString KoShapeFactoryBase::family() const
{
    return d->family;
}

int KoShapeFactoryBase::loadingPriority() const
{
    return d->loadingPriority;
}

PkList<std::pair<PkString, PkStringList> > KoShapeFactoryBase::odfElements() const
{
    return d->xmlElements;
}

void KoShapeFactoryBase::addTemplate(const KoShapeTemplate &params)
{
    KoShapeTemplate tmplate = params;
    tmplate.id = d->id;
    d->templates.append(tmplate);
}

void KoShapeFactoryBase::setToolTip(const PkString & tooltip)
{
    d->tooltip = tooltip;
}

void KoShapeFactoryBase::setIconName(const char *iconName)
{
    d->iconName = iconName;
}

void KoShapeFactoryBase::setFamily(const PkString & family)
{
    d->family = family;
}

PkString KoShapeFactoryBase::id() const
{
    return d->id;
}

PkList<KoShapeTemplate> KoShapeFactoryBase::templates() const
{
    return d->templates;
}

void KoShapeFactoryBase::setLoadingPriority(int priority)
{
    d->loadingPriority = priority;
}

void KoShapeFactoryBase::setXmlElementNames(const PkString & nameSpace, const PkStringList & names)
{
    d->xmlElements.clear();
    d->xmlElements.append(std::pair<PkString, PkStringList>(nameSpace, names));
}

void KoShapeFactoryBase::setXmlElements(const PkList<std::pair<PkString, PkStringList> > &elementNamesList)
{
    d->xmlElements = elementNamesList;
}

bool KoShapeFactoryBase::hidden() const
{
    return d->hidden;
}

void KoShapeFactoryBase::setHidden(bool hidden)
{
    d->hidden = hidden;
}

void KoShapeFactoryBase::newDocumentResourceManager(KoDocumentResourceManager *manager) const
{
    d->resourceManagers.append(manager);
    QObject::connect(manager, &QObject::destroyed, this, &KoShapeFactoryBase::pruneDocumentResourceManager);
}

KoShape *KoShapeFactoryBase::createDefaultShape(KoDocumentResourceManager *documentResources) const
{
    if (!d->deferredPluginName.isEmpty()) {
        const_cast<KoShapeFactoryBase*>(this)->getDeferredPlugin();
        Q_ASSERT(d->deferredFactory);
        if (d->deferredFactory) {
            return d->deferredFactory->createDefaultShape(documentResources);
        }
    }
    return 0;
}

KoShape *KoShapeFactoryBase::createShape(const KoProperties* properties,
                                         KoDocumentResourceManager *documentResources) const
{
    if (!d->deferredPluginName.isEmpty()) {
        const_cast<KoShapeFactoryBase*>(this)->getDeferredPlugin();
        Q_ASSERT(d->deferredFactory);
        if (d->deferredFactory) {
            return d->deferredFactory->createShape(properties, documentResources);
        }
    }
    return createDefaultShape(documentResources);
}

void KoShapeFactoryBase::getDeferredPlugin()
{
    // S-08: 插件加载已随 D-12 删除。deferredPluginName 恒为空（无子类传第三参），
    // deferredFactory 永不填充，本函数为 no-op；createShape/createDefaultShape 的
    // deferredFactory 分支同样不可达。
}

void KoShapeFactoryBase::pruneDocumentResourceManager(QObject *)
{
    PkList<PkPointer<KoDocumentResourceManager> > rms;
    Q_FOREACH(PkPointer<KoDocumentResourceManager> rm, d->resourceManagers) {
        if (rm) {
            rms << rm;
        }
    }
    d->resourceManagers = rms;
}
