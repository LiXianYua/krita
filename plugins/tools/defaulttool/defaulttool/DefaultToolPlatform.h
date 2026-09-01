/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Public host-neutral platform contract for the default-tool family.
 */
#pragma once

#include <PkList.h>
#include <PkString.h>

class DefaultTool;

enum class DefaultToolActionId {
    ObjectOrderFront,
    ObjectOrderRaise,
    ObjectOrderLower,
    ObjectOrderBack,
    ObjectAlignHorizontalLeft,
    ObjectAlignHorizontalCenter,
    ObjectAlignHorizontalRight,
    ObjectAlignVerticalTop,
    ObjectAlignVerticalCenter,
    ObjectAlignVerticalBottom,
    ObjectDistributeHorizontalLeft,
    ObjectDistributeHorizontalCenter,
    ObjectDistributeHorizontalRight,
    ObjectDistributeHorizontalGaps,
    ObjectDistributeVerticalTop,
    ObjectDistributeVerticalCenter,
    ObjectDistributeVerticalBottom,
    ObjectDistributeVerticalGaps,
    ObjectGroup,
    ObjectUngroup,
    ObjectTransformRotate90Clockwise,
    ObjectTransformRotate90CounterClockwise,
    ObjectTransformRotate180,
    ObjectTransformMirrorHorizontally,
    ObjectTransformMirrorVertically,
    ObjectTransformReset,
    ObjectUnite,
    ObjectIntersect,
    ObjectSubtract,
    ObjectSplit,
    TextTypePreformatted,
    TextTypePrePositioned,
    TextTypeInlineWrap,
    AddShapeToFlowArea,
    SubtractShapeFromFlowArea,
    PutTextOnPath,
    RemoveShapesFromTextFlow,
    FlowShapeTypeToggle,
    FlowShapeOrderBack,
    FlowShapeOrderEarlier,
    FlowShapeOrderLater,
    FlowShapeOrderFront,
    EditCut,
    EditCopy,
    EditPaste,
    PasteAt,
    ConvertShapesToVectorSelection
};

enum class DefaultToolActionScope {
    Tool,
    Host
};

enum class DefaultToolVariant {
    Default,
    ReferenceImages
};

struct DefaultToolActionDescriptor {
    DefaultToolActionId action;
    PkString actionId;
    DefaultToolActionScope scope {DefaultToolActionScope::Tool};
    bool availableInDefaultTool {true};
    bool availableInReferenceImages {false};
};

enum class DefaultToolMenuEntryKind {
    Section,
    Separator,
    Submenu,
    Action
};

struct DefaultToolMenuEntryDescriptor {
    DefaultToolMenuEntryKind kind {DefaultToolMenuEntryKind::Separator};
    PkString path;
    PkString label;
    DefaultToolActionId action {DefaultToolActionId::ObjectOrderFront};
};

enum class DefaultToolCursorKind {
    Arrow,
    Move,
    ResizeVertical,
    ResizeBackwardDiagonal,
    ResizeHorizontal,
    ResizeForwardDiagonal,
    Rotate,
    Shear
};

struct DefaultToolCursorDescriptor {
    DefaultToolCursorKind kind {DefaultToolCursorKind::Arrow};
    int slot {0};
    PkString resource;
    int angle {0};
};

class DefaultToolPlatformServices
{
public:
    virtual ~DefaultToolPlatformServices() = default;

    virtual bool dispatchDefaultToolHostAction(DefaultToolActionId action, bool checked) = 0;
    virtual void useDefaultToolCursor(const DefaultToolCursorDescriptor &cursor) = 0;
};

const PkList<DefaultToolActionDescriptor> &defaultToolActionDescriptors();
const DefaultToolActionDescriptor *defaultToolActionDescriptor(DefaultToolActionId action);
PkList<DefaultToolMenuEntryDescriptor> defaultToolMenuDescriptors(DefaultToolVariant variant);
const PkList<DefaultToolCursorDescriptor> &defaultToolCursorDescriptors();

bool dispatchDefaultToolAction(DefaultTool *tool, DefaultToolActionId action, bool checked = false);
