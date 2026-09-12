/*
 *  SPDX-FileCopyrightText: 2024 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KOSVGTEXTPROPERTIESINTERFACE_H
#define KOSVGTEXTPROPERTIESINTERFACE_H

#include <PkObject.h>
#include <KoSvgTextProperties.h>
#include <kritaflake_export.h>

/**
 * @brief The KoSvgTextPropertiesInterface class
 *
 * This is an interface that can be used by tools to communicate
 * with the KisTextPropertiesManager.
 *
 * D-B（2026-09-12）：本类是裁决 D-B 的**第四个实例**（前三个：KoToolSelection /
 * KoToolProxy / KoPathToolSelection，R-57 已剥）。此前它是**无条件双身份**
 * `public QObject, public PkObject`——QObject 那一半（宿主对象树 / QPointer /
 * 事件循环 / 元对象）本次被移除，投递只剩 Pk 一条
 * （`activateSignal<PkMemberFnKey>`，与 QObject 无关），于是这里只剩
 * `public PkObject`，布局与 mangled 拼法全树统一。
 */
class KRITAFLAKE_EXPORT KoSvgTextPropertiesInterface : public PkObject
{
public:
    explicit KoSvgTextPropertiesInterface(PkObject *parent = nullptr): PkObject(parent){}

    /**
     * @brief getSelectedProperties
     * @return all KoSvgTextProperties for the given selection.
     */
    virtual PkList<KoSvgTextProperties> getSelectedProperties() = 0;

    /**
     * @brief getSelectedProperties
     * @return all KoSvgTextProperties for the given character selection.
     */
    virtual PkList<KoSvgTextProperties> getCharacterProperties()  = 0;

    /**
     * @brief getInheritedProperties
     * The properties that should be visible when a given property
     * isn't available in common properties. This is typically the
     * paragraph properties.
     * @return what counts as the inherited properties for the given selection.
     */
    virtual KoSvgTextProperties getInheritedProperties() = 0;

    /**
     * @brief setPropertiesOnSelected
     * This sets the properties on the selection. The implementation is responsible
     * for handling the undo states.
     * @param properties -- the properties to set.
     * @param removeProperties -- properties to remove.
     */
    virtual void setPropertiesOnSelected(KoSvgTextProperties properties, PkSet<KoSvgTextProperties::PropertyId> removeProperties = PkSet<KoSvgTextProperties::PropertyId>()) = 0;

    /**
     * @brief setCharacterPropertiesOnSelected
     * This sets the properties for a character selection instead of the full
     * text shape. Typically the selection of characters.
     * The implementation is responsible for handling the undo states.
     * @param properties -- the properties to set.
     * @param removeProperties -- properties to remove.
     */
    virtual void setCharacterPropertiesOnSelected(KoSvgTextProperties properties, PkSet<KoSvgTextProperties::PropertyId> removeProperties = PkSet<KoSvgTextProperties::PropertyId>()) = 0;

    /// Whether the tool is currently selecting a set of characters instead of whole paragraphs.
    virtual bool spanSelection() = 0;

    /// Whether character selections are possible at all.
    virtual bool characterPropertiesEnabled() = 0;
public:
    /// Emit to signal to KisTextPropertiesManager to call getSelectedProperties
    void textSelectionChanged()
    {
        activateSignal<>(this,
                         PkMemberFnKey::from(&KoSvgTextPropertiesInterface::textSelectionChanged));
    }
    /// Emit to signal to KisTextPropertiesManager to call getCharacterProperties
    /// and getInheritedProperties.
    void textCharacterSelectionChanged()
    {
        activateSignal<>(this,
                         PkMemberFnKey::from(&KoSvgTextPropertiesInterface::textCharacterSelectionChanged));
    }
};


#endif // KOSVGTEXTPROPERTIESINTERFACE_H
