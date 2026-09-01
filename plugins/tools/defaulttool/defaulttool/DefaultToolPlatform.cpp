/* SPDX-License-Identifier: LGPL-2.0-or-later */

#include "DefaultToolPlatform.h"

#include "DefaultTool.h"

namespace {

using Action = DefaultToolActionId;
using Scope = DefaultToolActionScope;
using Entry = DefaultToolMenuEntryDescriptor;
using EntryKind = DefaultToolMenuEntryKind;

Entry section(const char *label)
{
    return {EntryKind::Section, {}, PkString(label), Action::ObjectOrderFront};
}

Entry separator(const char *path = "")
{
    return {EntryKind::Separator, PkString(path), {}, Action::ObjectOrderFront};
}

Entry submenu(const char *path, const char *label)
{
    return {EntryKind::Submenu, PkString(path), PkString(label), Action::ObjectOrderFront};
}

Entry action(const char *path, Action id)
{
    return {EntryKind::Action, PkString(path), {}, id};
}

}

const PkList<DefaultToolActionDescriptor> &defaultToolActionDescriptors()
{
    static const PkList<DefaultToolActionDescriptor> descriptors {
        {Action::ObjectOrderFront, "object_order_front", Scope::Tool, true, true},
        {Action::ObjectOrderRaise, "object_order_raise", Scope::Tool, true, true},
        {Action::ObjectOrderLower, "object_order_lower", Scope::Tool, true, true},
        {Action::ObjectOrderBack, "object_order_back", Scope::Tool, true, true},
        {Action::ObjectAlignHorizontalLeft, "object_align_horizontal_left", Scope::Tool, true, false},
        {Action::ObjectAlignHorizontalCenter, "object_align_horizontal_center", Scope::Tool, true, false},
        {Action::ObjectAlignHorizontalRight, "object_align_horizontal_right", Scope::Tool, true, false},
        {Action::ObjectAlignVerticalTop, "object_align_vertical_top", Scope::Tool, true, false},
        {Action::ObjectAlignVerticalCenter, "object_align_vertical_center", Scope::Tool, true, false},
        {Action::ObjectAlignVerticalBottom, "object_align_vertical_bottom", Scope::Tool, true, false},
        {Action::ObjectDistributeHorizontalLeft, "object_distribute_horizontal_left", Scope::Tool, true, false},
        {Action::ObjectDistributeHorizontalCenter, "object_distribute_horizontal_center", Scope::Tool, true, false},
        {Action::ObjectDistributeHorizontalRight, "object_distribute_horizontal_right", Scope::Tool, true, false},
        {Action::ObjectDistributeHorizontalGaps, "object_distribute_horizontal_gaps", Scope::Tool, true, false},
        {Action::ObjectDistributeVerticalTop, "object_distribute_vertical_top", Scope::Tool, true, false},
        {Action::ObjectDistributeVerticalCenter, "object_distribute_vertical_center", Scope::Tool, true, false},
        {Action::ObjectDistributeVerticalBottom, "object_distribute_vertical_bottom", Scope::Tool, true, false},
        {Action::ObjectDistributeVerticalGaps, "object_distribute_vertical_gaps", Scope::Tool, true, false},
        {Action::ObjectGroup, "object_group", Scope::Tool, true, false},
        {Action::ObjectUngroup, "object_ungroup", Scope::Tool, true, false},
        {Action::ObjectTransformRotate90Clockwise, "object_transform_rotate_90_cw", Scope::Tool, true, true},
        {Action::ObjectTransformRotate90CounterClockwise, "object_transform_rotate_90_ccw", Scope::Tool, true, true},
        {Action::ObjectTransformRotate180, "object_transform_rotate_180", Scope::Tool, true, true},
        {Action::ObjectTransformMirrorHorizontally, "object_transform_mirror_horizontally", Scope::Tool, true, true},
        {Action::ObjectTransformMirrorVertically, "object_transform_mirror_vertically", Scope::Tool, true, true},
        {Action::ObjectTransformReset, "object_transform_reset", Scope::Tool, true, true},
        {Action::ObjectUnite, "object_unite", Scope::Tool, true, false},
        {Action::ObjectIntersect, "object_intersect", Scope::Tool, true, false},
        {Action::ObjectSubtract, "object_subtract", Scope::Tool, true, false},
        {Action::ObjectSplit, "object_split", Scope::Tool, true, false},
        {Action::TextTypePreformatted, "text_type_preformatted", Scope::Tool, true, false},
        {Action::TextTypePrePositioned, "text_type_pre_positioned", Scope::Tool, true, false},
        {Action::TextTypeInlineWrap, "text_type_inline_wrap", Scope::Tool, true, false},
        {Action::AddShapeToFlowArea, "add_shape_to_flow_area", Scope::Tool, true, false},
        {Action::SubtractShapeFromFlowArea, "subtract_shape_from_flow_area", Scope::Tool, true, false},
        {Action::PutTextOnPath, "put_text_on_path", Scope::Tool, true, false},
        {Action::RemoveShapesFromTextFlow, "remove_shapes_from_text_flow", Scope::Tool, true, false},
        {Action::FlowShapeTypeToggle, "flow_shape_type_toggle", Scope::Tool, true, false},
        {Action::FlowShapeOrderBack, "flow_shape_order_back", Scope::Tool, true, false},
        {Action::FlowShapeOrderEarlier, "flow_shape_order_earlier", Scope::Tool, true, false},
        {Action::FlowShapeOrderLater, "flow_shape_order_later", Scope::Tool, true, false},
        {Action::FlowShapeOrderFront, "flow_shape_order_front", Scope::Tool, true, false},
        {Action::EditCut, "edit_cut", Scope::Host, true, true},
        {Action::EditCopy, "edit_copy", Scope::Host, true, true},
        {Action::EditPaste, "edit_paste", Scope::Host, true, true},
        {Action::PasteAt, "paste_at", Scope::Host, true, false},
        {Action::ConvertShapesToVectorSelection, "convert_shapes_to_vector_selection", Scope::Host, true, false}
    };
    return descriptors;
}

const DefaultToolActionDescriptor *defaultToolActionDescriptor(DefaultToolActionId actionId)
{
    for (const auto &descriptor : defaultToolActionDescriptors()) {
        if (descriptor.action == actionId) return &descriptor;
    }
    return nullptr;
}

PkList<DefaultToolMenuEntryDescriptor> defaultToolMenuDescriptors(DefaultToolVariant variant)
{
    if (variant == DefaultToolVariant::ReferenceImages) {
        return {
            section("Reference Image Actions"), separator(), submenu("Transform", "Transform"),
            action("Transform", Action::ObjectTransformRotate90Clockwise),
            action("Transform", Action::ObjectTransformRotate90CounterClockwise),
            action("Transform", Action::ObjectTransformRotate180), separator("Transform"),
            action("Transform", Action::ObjectTransformMirrorHorizontally),
            action("Transform", Action::ObjectTransformMirrorVertically), separator("Transform"),
            action("Transform", Action::ObjectTransformReset), separator(),
            action("", Action::EditCut), action("", Action::EditCopy), action("", Action::EditPaste),
            separator(), action("", Action::ObjectOrderFront), action("", Action::ObjectOrderRaise),
            action("", Action::ObjectOrderLower), action("", Action::ObjectOrderBack)
        };
    }

    return {
        section("Vector Shape Actions"), separator(), submenu("Transform", "Transform"),
        action("Transform", Action::ObjectTransformRotate90Clockwise),
        action("Transform", Action::ObjectTransformRotate90CounterClockwise),
        action("Transform", Action::ObjectTransformRotate180), separator("Transform"),
        action("Transform", Action::ObjectTransformMirrorHorizontally),
        action("Transform", Action::ObjectTransformMirrorVertically), separator("Transform"),
        action("Transform", Action::ObjectTransformReset),
        submenu("Logical Operations", "Logical Operations"),
        action("Logical Operations", Action::ObjectUnite), action("Logical Operations", Action::ObjectIntersect),
        action("Logical Operations", Action::ObjectSubtract), action("Logical Operations", Action::ObjectSplit),
        separator(), action("", Action::EditCut), action("", Action::EditCopy), action("", Action::EditPaste),
        action("", Action::PasteAt), separator(), action("", Action::ObjectOrderFront),
        action("", Action::ObjectOrderRaise), action("", Action::ObjectOrderLower), action("", Action::ObjectOrderBack),
        separator(), action("", Action::ObjectGroup), action("", Action::ObjectUngroup), separator(),
        action("", Action::ConvertShapesToVectorSelection), separator(), submenu("Text", "Text"),
        action("Text", Action::AddShapeToFlowArea), action("Text", Action::SubtractShapeFromFlowArea),
        action("Text", Action::PutTextOnPath), separator("Text"), action("Text", Action::TextTypePreformatted),
        action("Text", Action::TextTypeInlineWrap), action("Text", Action::TextTypePrePositioned), separator("Text"),
        action("Text", Action::RemoveShapesFromTextFlow), action("Text", Action::FlowShapeTypeToggle), separator("Text"),
        action("Text", Action::FlowShapeOrderBack), action("Text", Action::FlowShapeOrderEarlier),
        action("Text", Action::FlowShapeOrderLater), action("Text", Action::FlowShapeOrderFront)
    };
}

const PkList<DefaultToolCursorDescriptor> &defaultToolCursorDescriptors()
{
    static const PkList<DefaultToolCursorDescriptor> descriptors {
        {DefaultToolCursorKind::ResizeVertical, 0, {}, 0},
        {DefaultToolCursorKind::ResizeBackwardDiagonal, 1, {}, 0},
        {DefaultToolCursorKind::ResizeHorizontal, 2, {}, 0},
        {DefaultToolCursorKind::ResizeForwardDiagonal, 3, {}, 0},
        {DefaultToolCursorKind::ResizeVertical, 4, {}, 0},
        {DefaultToolCursorKind::ResizeBackwardDiagonal, 5, {}, 0},
        {DefaultToolCursorKind::ResizeHorizontal, 6, {}, 0},
        {DefaultToolCursorKind::ResizeForwardDiagonal, 7, {}, 0},
        {DefaultToolCursorKind::Rotate, 0, "cursor_rotate.png", 45},
        {DefaultToolCursorKind::Rotate, 1, "cursor_rotate.png", 90},
        {DefaultToolCursorKind::Rotate, 2, "cursor_rotate.png", 135},
        {DefaultToolCursorKind::Rotate, 3, "cursor_rotate.png", 180},
        {DefaultToolCursorKind::Rotate, 4, "cursor_rotate.png", 225},
        {DefaultToolCursorKind::Rotate, 5, "cursor_rotate.png", 270},
        {DefaultToolCursorKind::Rotate, 6, "cursor_rotate.png", 315},
        {DefaultToolCursorKind::Rotate, 7, "cursor_rotate.png", 0},
        {DefaultToolCursorKind::Shear, 0, "cursor_shear.png", 0},
        {DefaultToolCursorKind::Shear, 1, "cursor_shear.png", 45},
        {DefaultToolCursorKind::Shear, 2, "cursor_shear.png", 90},
        {DefaultToolCursorKind::Shear, 3, "cursor_shear.png", 135},
        {DefaultToolCursorKind::Shear, 4, "cursor_shear.png", 180},
        {DefaultToolCursorKind::Shear, 5, "cursor_shear.png", 225},
        {DefaultToolCursorKind::Shear, 6, "cursor_shear.png", 270},
        {DefaultToolCursorKind::Shear, 7, "cursor_shear.png", 315}
    };
    return descriptors;
}

bool dispatchDefaultToolAction(DefaultTool *tool, DefaultToolActionId actionId, bool checked)
{
    return tool && tool->dispatchAction(actionId, checked);
}
