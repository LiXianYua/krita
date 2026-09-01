/* This file is part of the KDE project
 *
 * SPDX-FileCopyrightText: 2006-2007 Thomas Zander <zander@kde.org>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "DefaultToolFactory.h"
#include "DefaultTool.h"



DefaultToolFactory::DefaultToolFactory()
    : KoToolFactoryBase(KoInteractionTool_ID)
{
    setToolTip(PkString("Select Shapes Tool"));
    setSection(ToolBoxSection::Main);
    setPriority(0);
    setActivationShapeId("flake/always");
}

DefaultToolFactory::DefaultToolFactory(const PkString &id)
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

PkList<DefaultToolAction *> DefaultToolFactory::createActionsImpl()
{

    PkList<DefaultToolAction *> actions;
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_order_front"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_order_raise"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_order_lower"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_order_back"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_align_horizontal_left"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_align_horizontal_center"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_align_horizontal_right"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_align_vertical_top"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_align_vertical_center"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_align_vertical_bottom"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_distribute_horizontal_left"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_distribute_horizontal_center"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_distribute_horizontal_right"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_distribute_horizontal_gaps"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_distribute_vertical_top"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_distribute_vertical_center"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_distribute_vertical_bottom"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_distribute_vertical_gaps"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_group"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_ungroup"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_transform_rotate_90_cw"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_transform_rotate_90_ccw"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_transform_rotate_180"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_transform_mirror_horizontally"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_transform_mirror_vertically"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_transform_reset"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_unite"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_intersect"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_subtract"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("object_split"); actions << action; }

    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("text_type_preformatted"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("text_type_pre_positioned"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("text_type_inline_wrap"); actions << action; }

    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("add_shape_to_flow_area"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("subtract_shape_from_flow_area"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("put_text_on_path"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("remove_shapes_from_text_flow"); actions << action; }

    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("flow_shape_type_toggle"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("flow_shape_order_back"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("flow_shape_order_earlier"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("flow_shape_order_later"); actions << action; }
    { DefaultToolAction *action = new DefaultToolAction(); action->setObjectName("flow_shape_order_front"); actions << action; }

    return actions;

}
