/*
 * SPDX-License-Identifier: LGPL-2.0-or-later
 *
 * Small host-neutral carriers for the default-tool action, menu, cursor and
 * key-input contracts.  They deliberately contain no desktop toolkit API.
 */
#pragma once

#include <PkList.h>
#include <PkObject.h>
#include <PkString.h>

#include "DefaultToolPlatform.h"

class DefaultToolActionGroup;

class DefaultToolAction : public PkObject
{
public:
    explicit DefaultToolAction(PkObject *parent = nullptr)
        : PkObject(parent)
    {
    }

    void setEnabled(bool value) { m_enabled = value; }
    bool isEnabled() const { return m_enabled; }
    void setCheckable(bool value) { m_checkable = value; }
    bool isCheckable() const { return m_checkable; }
    void setChecked(bool value) { m_checked = value; }
    bool isChecked() const { return m_checked; }
    DefaultToolActionGroup *actionGroup() const { return m_group; }
    void setActionGroup(DefaultToolActionGroup *group) { m_group = group; }

    void triggered()
    {
        activateSignal<>(this, PkMemberFnKey::from(&DefaultToolAction::triggered));
    }

private:
    bool m_enabled {true};
    bool m_checkable {false};
    bool m_checked {false};
    DefaultToolActionGroup *m_group {nullptr};
};

class DefaultToolActionGroup : public PkObject
{
public:
    explicit DefaultToolActionGroup(PkObject *parent = nullptr)
        : PkObject(parent)
    {
    }

    void addAction(DefaultToolAction *action)
    {
        if (!action) return;
        m_actions.append(action);
        action->setActionGroup(this);
    }
    const PkList<DefaultToolAction *> &actions() const { return m_actions; }
    void setExclusive(bool value) { m_exclusive = value; }
    void setEnabled(bool value)
    {
        for (DefaultToolAction *action : m_actions) action->setEnabled(value);
    }

private:
    PkList<DefaultToolAction *> m_actions;
    bool m_exclusive {true};
};

class DefaultToolMenu
#ifdef DEFAULTTOOL_SHELL
    : public ShellMenu
#endif
{
public:
    explicit DefaultToolMenu(const PkString &title = {}) : m_title(title) {}
    void clear() { m_entries.clear(); m_children.clear(); }
    void addSection(const PkString &title) { m_entries.append(PkString("section:") + title); }
    void addSeparator() { m_entries.append(PkString("separator")); }
    DefaultToolMenu *addMenu(const PkString &title)
    {
        m_children.append(DefaultToolMenu(title));
        m_entries.append(PkString("menu:") + title);
        return &m_children.last();
    }
    void addAction(DefaultToolAction *action)
    {
        if (action) m_entries.append(action->objectName());
    }
    const PkList<PkString> &entries() const { return m_entries; }

private:
    PkString m_title;
    PkList<PkString> m_entries;
    PkList<DefaultToolMenu> m_children;
};

class DefaultToolCursor
#ifdef DEFAULTTOOL_SHELL
    : public ShellCursor
#endif
{
public:
    DefaultToolCursor() = default;
    DefaultToolCursor(Qt::CursorShape shape)
        : m_descriptor(cursorDescriptor(shape)) {}
    explicit DefaultToolCursor(const DefaultToolCursorDescriptor &descriptor)
        : m_descriptor(descriptor) {}
    DefaultToolCursor(const PkString &resource, int angle = 0)
        : m_descriptor({DefaultToolCursorKind::Rotate, 0, resource, angle}) {}
    const PkString &resource() const { return m_descriptor.resource; }
    int angle() const { return m_descriptor.angle; }
    const DefaultToolCursorDescriptor &descriptor() const { return m_descriptor; }

private:
    static DefaultToolCursorDescriptor cursorDescriptor(Qt::CursorShape shape)
    {
        switch (shape) {
        case Qt::SizeAllCursor:
            return {DefaultToolCursorKind::Move, 0, {}, 0};
        case Qt::SizeVerCursor:
            return {DefaultToolCursorKind::ResizeVertical, 0, {}, 0};
        case Qt::SizeBDiagCursor:
            return {DefaultToolCursorKind::ResizeBackwardDiagonal, 0, {}, 0};
        case Qt::SizeHorCursor:
            return {DefaultToolCursorKind::ResizeHorizontal, 0, {}, 0};
        case Qt::SizeFDiagCursor:
            return {DefaultToolCursorKind::ResizeForwardDiagonal, 0, {}, 0};
        default:
            return {DefaultToolCursorKind::Arrow, 0, {}, 0};
        }
    }

    DefaultToolCursorDescriptor m_descriptor;
};

class DefaultToolKeyEvent
#ifdef DEFAULTTOOL_SHELL
    : public ShellKeyEvent
#endif
{
public:
    DefaultToolKeyEvent(int key, Qt::KeyboardModifiers modifiers = {})
        : m_key(key), m_modifiers(modifiers) {}
    int key() const { return m_key; }
    Qt::KeyboardModifiers modifiers() const { return m_modifiers; }
    void accept() { m_accepted = true; }
    bool isAccepted() const { return m_accepted; }

private:
    int m_key;
    Qt::KeyboardModifiers m_modifiers;
    bool m_accepted {false};
};
