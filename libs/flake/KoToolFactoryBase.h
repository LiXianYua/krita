/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2004 Boudewijn Rempt <boud@valdyas.org>
 * SPDX-FileCopyrightText: 2006 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#ifndef KO_TOOL_FACTORY_H
#define KO_TOOL_FACTORY_H

#include "kritaflake_export.h"

#include <PkString.h>
#include <PkList.h>
#include <PkNamespace.h>
#include <PkKeySequence.h>
#include <QObject>

class KoCanvasBase;
class KoToolBase;
class QAction;

/**
 * Each tool has a "section" which it uses to be grouped in the toolbox.
 */
namespace ToolBoxSection {
    static const PkString Main {"main"};                   ///< Tools that only work on vector shapes
    static const PkString Shape {"0 Krita/Shape"};         ///< Freehand and shapes like ellipses and lines
    static const PkString Transform {"2 Krita/Transform"}; ///< Tools that transform the layer
    static const PkString Fill {"3 Krita/Fill"};           ///< Tools that fill parts of the canvas
    static const PkString View {"4 Krita/View"};           ///< Assistance tools: guides, reference, etc.
    static const PkString Select {"5 Krita/Select"};       ///< Tools that select pixels
    static const PkString Navigation {"navigation"};       ///< Tools that affect the canvas: pan, zoom, etc.
}

/**
 * A factory for KoToolBase objects.
 *
 * The baseclass for all tool plugins. Each plugin that ships a KoToolBase should also
 * ship a factory. That factory will extend this class and set variable data like
 * a toolTip and icon in the constructor of that extending class.
 *
 * An example usage would be:<pre>
 * class MyToolFactory : public KoToolFactoryBase {
 * public:
 *   MyToolFactory(const PkStringList&)
 *       : KoToolFactoryBase("MyTool") {
 *       setToolTip(i18n("Create object"));
 *       setSection("main");
 *       setPriority(5);
 *   }
 *   ~MyToolFactory() {}
 *   KoToolBase *createTool(KoCanvasBase *canvas);
 * };
 * K_PLUGIN_FACTORY_WITH_JSON((MyToolFactoryFactory, "mytool.json", registerPlugin<MyToolFactory>();)
</pre>

 */
class KRITAFLAKE_EXPORT KoToolFactoryBase : public QObject
{
public:
    /**
     * Create the new factory
     * @param id a string that will be used internally for referencing the tool
     */
    explicit KoToolFactoryBase(const PkString &id);
    virtual ~KoToolFactoryBase();

    /**
     * Create the actions for this tool. Actions are unique per window, not per
     * tool instance; tool instances are unique per view/canvas.
     *
     * @p actionCollection is a plain QObject used as an action repository.
     * Every QAction added to it must be parented to the collection
     * (setParent(actionCollection)) and carry the action name as its
     * objectName (setObjectName(name)) — actions are looked up afterwards via
     * findChild<QAction *>(name)/findChildren<QAction *>() on that objectName.
     */
    PkList<QAction *> createActions(QObject *actionCollection);

    /**
     * Instantiate a new tool
     * @param canvas the canvas that the new tool will work on. Should be passed
     *    to the constructor of the tool.
     * @return a new KoToolBase instance, or zero if the tool doesn't want to show up.
     */
    virtual KoToolBase *createTool(KoCanvasBase *canvas) = 0;

    /**
     * return the id for the tool this factory creates.
     * @return the id for the tool this factory creates.
     */
    PkString id() const;
    /**
     * Returns The priority of this tool in its section in the toolbox
     * @return The priority of this tool.
     */
    int priority() const;
    /**
     * returns the section used to group tools in the toolbox
     * @return the section
     */
    PkString section() const;
    /**
     * return a translated tooltip Text
     * @return a translated tooltip Text
     */
    PkString toolTip() const;
    /**
     * return the basename of the icon for this tool
     * @return the basename of the icon for this tool
     */
    PkString iconName() const;

    /**
     * Return the id of the shape we can process.
     * This is the shape Id the tool we create is associated with.  So a TextTool for a TextShape.
     * @see KoShapeFactoryBase::shapeId()
     * @see setActivationShapeId()
     * @return the id of a shape, or an empty string for all shapes.
     */
    PkString activationShapeId() const;

    /**
     * Return the default keyboard shortcut for activation of this tool (if
     * the shape this tool belongs to is active).
     *
     * See KoToolManager for use.
     *
     * @return the shortcut
     */
    PkKeySequence shortcut() const;

protected:

    /**
     * Set the default shortcut for activation of this tool.
     */
    void setShortcut(const PkKeySequence &shortcut);

    /**
     * Set the tooltip to be used for this tool
     * @param tooltip the tooltip
     */
    void setToolTip(const PkString &tooltip);

    /**
     * Set the section used to group tools in the toolbox
     * @param section the section
     */
    void setSection(const PkString &section);

    /**
     * Set an icon to be used in the toolBox.
     * @param iconName the basename (without extension) of the icon
     */
    void setIconName(const char *iconName);
    void setIconName(const PkString &iconName);

    /**
     * Set the priority of this tool, as it is shown in the toolBox; lower number means
     * it will be show more to the front of the list.
     * @param newPriority the priority
     */
    void setPriority(int newPriority);

    /**
     * Set the id of the shape we can process.
     * This is the Id, as passed to the constructor of a KoShapeFactoryBase, that the tool
     * we create is associated with. This means that if a KoTextShape is selected, then
     * all tools that have its id set here will be added to the dynamic part of the toolbox.
     * @param activationShapeId the Id of the shape
     * @see activationShapeId()
     */
    void setActivationShapeId(const PkString &activationShapeId);

    /**
     * @brief createActionsImpl should be reimplemented if the tool needs any actions.
     * The actions should have a valid objectName().
     *
     * @return the list of actions this tool wishes to be available.
     */
    virtual PkList<QAction *> createActionsImpl();

    /** Host boundary for factories that keep only QAction identity. */
    QAction *createHostAction(const char *text,
                              const PkString &objectName,
                              Pk::Key shortcut = static_cast<Pk::Key>(0));

private:
    class Private;
    Private * const d;
};

#endif
