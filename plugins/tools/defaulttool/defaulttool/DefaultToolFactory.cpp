/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "DefaultToolFactory.h"
#include "DefaultTool.h"


#include <QAction>
#include <klocalizedstring.h>

DefaultToolFactory::DefaultToolFactory()
    : KoToolFactoryBase(KoInteractionTool_ID)
{
    setToolTip(i18n("Select Shapes Tool"));
    setSection(ToolBoxSection::Main);
    setPriority(0);
    setActivationShapeId("flake/always");
}

DefaultToolFactory::DefaultToolFactory(const QString &id)
    : KoToolFactoryBase(id)
{
}

DefaultToolFactory::~DefaultToolFactory()
{
}

KoToolBase *DefaultToolFactory::createTool(KoCanvasBase *canvas)
{
    return new DefaultTool(canvas, true);
}

QList<QAction *> DefaultToolFactory::createActionsImpl()
{

    QList<QAction *> actions;
    { QAction *action = new QAction(this); action->setObjectName("object_order_front"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_order_raise"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_order_lower"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_order_back"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_align_horizontal_left"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_align_horizontal_center"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_align_horizontal_right"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_align_vertical_top"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_align_vertical_center"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_align_vertical_bottom"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_distribute_horizontal_left"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_distribute_horizontal_center"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_distribute_horizontal_right"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_distribute_horizontal_gaps"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_distribute_vertical_top"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_distribute_vertical_center"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_distribute_vertical_bottom"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_distribute_vertical_gaps"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_group"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_ungroup"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_transform_rotate_90_cw"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_transform_rotate_90_ccw"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_transform_rotate_180"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_transform_mirror_horizontally"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_transform_mirror_vertically"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_transform_reset"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_unite"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_intersect"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_subtract"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("object_split"); actions << action; }

    { QAction *action = new QAction(this); action->setObjectName("text_type_preformatted"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("text_type_pre_positioned"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("text_type_inline_wrap"); actions << action; }

    { QAction *action = new QAction(this); action->setObjectName("add_shape_to_flow_area"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("subtract_shape_from_flow_area"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("put_text_on_path"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("remove_shapes_from_text_flow"); actions << action; }

    { QAction *action = new QAction(this); action->setObjectName("flow_shape_type_toggle"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("flow_shape_order_back"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("flow_shape_order_earlier"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("flow_shape_order_later"); actions << action; }
    { QAction *action = new QAction(this); action->setObjectName("flow_shape_order_front"); actions << action; }

    return actions;

}
