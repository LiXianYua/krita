/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2006 Boudewijn Rempt (boud@valdyas.org)
 * SPDX-FileCopyrightText: 2006-2007, 2010 Thomas Zander <zander@kde.org>
 * SPDX-FileCopyrightText: 2006, 2008-2010 Thorsten Zachmann <zachmann@kde.org>
 * SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
 * SPDX-FileCopyrightText: 2010 Inge Wallin <inge@lysator.liu.se>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

// Own
#include "KoShapeRegistry.h"

#include "KoSvgTextShape.h"
#include "KoPathShapeFactory.h"
#include "shapes/ImageShapeFactory.h"
#include "shapes/RectangleShapeFactory.h"
#include "KoShapeLoadingContext.h"
#include "KoShapeSavingContext.h"
#include "KoShapeGroup.h"
#include "KoShapeLayer.h"

#include <KoXmlNS.h>

#include <PkString.h>
#include <PkHash.h>

#include <iterator>
#include <map>

#include <FlakeDebug.h>

class KoShapeRegistry::Private
{
public:
    void insertFactory(KoShapeFactoryBase *factory);
    void init(KoShapeRegistry *q);

    // Map namespace,tagname to priority:factory
    PkHash<std::pair<PkString, PkString>, std::multimap<int, KoShapeFactoryBase*> > factoryMap;
};

KoShapeRegistry::KoShapeRegistry()
        : d(new Private())
{
    d->init(this);
}

KoShapeRegistry::~KoShapeRegistry()
{
    for (KoShapeFactoryBase *factory : doubleEntries()) {
        delete factory;
    }
    for (KoShapeFactoryBase *factory : values()) {
        delete factory;
    }
    delete d;
}

void KoShapeRegistry::Private::init(KoShapeRegistry *q)
{
    // S-08: 插件加载已随 D-12 删除，只保留硬编码 factory。

    // Also add our hard-coded basic shapes
    KoShapeFactoryBase *svgTextFactory = new KoSvgTextShapeFactory();
    q->add(svgTextFactory->id(), svgTextFactory);
    KoShapeFactoryBase *pathFactory = new KoPathShapeFactory(PkStringList());
    q->add(pathFactory->id(), pathFactory);
    // S-08: ImageShape/RectangleShape 原由 Krita/Shape 插件提供，D-12 删插件加载后
    // 在此硬编码补注册（D-07 崩溃根因 + TestKoDrag 缺 rect 工厂）。
    KoShapeFactoryBase *imageFactory = new ImageShapeFactory();
    q->add(imageFactory->id(), imageFactory);
    KoShapeFactoryBase *rectFactory = new RectangleShapeFactory();
    q->add(rectFactory->id(), rectFactory);

    // Now all shape factories are registered with us, determine their
    // associated odf tagname & priority and prepare ourselves for
    // loading ODF.

    PkList<KoShapeFactoryBase*> factories = q->values();
    for (int i = 0; i < factories.size(); ++i) {
        insertFactory(factories[i]);
    }
}

KoShapeRegistry* KoShapeRegistry::instance()
{
    static KoShapeRegistry registry;
    return &registry;
}

void KoShapeRegistry::addFactory(KoShapeFactoryBase * factory)
{
    add(factory->id(), factory);
    d->insertFactory(factory);
}

void KoShapeRegistry::Private::insertFactory(KoShapeFactoryBase *factory)
{
    const PkList<std::pair<PkString, PkStringList> > odfElements(factory->odfElements());

    if (odfElements.isEmpty()) {
        debugFlake << "Shape factory" << factory->id() << " does not have OdfNamespace defined, ignoring";
    }
    else {
        int priority = factory->loadingPriority();
        for (PkList<std::pair<PkString, PkStringList> >::const_iterator it(odfElements.begin()); it != odfElements.end(); ++it) {
            for (const PkString &elementName : (*it).second) {
                std::pair<PkString, PkString> p((*it).first, elementName);

                std::multimap<int, KoShapeFactoryBase*> &priorityMap = factoryMap[p];

                priorityMap.emplace(priority, factory);

                debugFlake << "Inserting factory" << factory->id() << " for"
                    << p << " with priority "
                    << priority << " into factoryMap making "
                    << priorityMap.size() << " entries. ";
            }
        }
    }
}

#include "kis_debug.h"
#include <KoUnit.h>
#include <KoDocumentResourceManager.h>
#include <KoShapeController.h>
#include <KoShapeGroupCommand.h>


PkList<KoShapeFactoryBase*> KoShapeRegistry::factoriesForElement(const PkString &nameSpace, const PkString &elementName)
{
    // Pair of namespace, tagname
    std::pair<PkString, PkString> p = std::pair<PkString, PkString>(nameSpace, elementName);

    const std::multimap<int, KoShapeFactoryBase*> priorityMap = d->factoryMap.value(p);
    PkList<KoShapeFactoryBase*> shapeFactories;
    // 旧写法 `Q_FOREACH (f, priorityMap.values()) shapeFactories.prepend(f);` 的净效果 =
    // 「优先级降序，**同优先级内插入序**」：QMultiMap 的相等键迭代是逆插入序、values()
    // 按键升序，逐项 prepend 把两者都翻了回来。std::multimap 的 equal_range 本来就是
    // 插入序，所以只需按**键组降序**走、组内保持正序。
    auto groupEnd = priorityMap.end();
    while (groupEnd != priorityMap.begin()) {
        const int priority = std::prev(groupEnd)->first;
        const auto range = priorityMap.equal_range(priority);
        for (auto it = range.first; it != range.second; ++it) {
            shapeFactories.append(it->second);
        }
        groupEnd = range.first;
    }

    return shapeFactories;
}
