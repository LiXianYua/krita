/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SVG_TEXT_QT_PLATFORM_HOST_H
#define SVG_TEXT_QT_PLATFORM_HOST_H

#include <KoCanvasKeyBindingHost.h>
#include <KoCanvasPlatformHost.h>
#include <PkColor.h>
#include <PkKeySequence.h>
#include <PkNamespace.h>
#include <PkString.h>

class KoCanvasBase;

/**
 * Qt-side reference adapter for the two host faces the svg text tool consumes:
 * the platform query face (KoCanvasPlatformHost) and the key-binding face
 * (KoCanvasKeyBindingHost).
 *
 * It is the second implementation of those ports — the answers still come from
 * the real Qt platform, moved out of the retained static target so the core
 * side holds no Qt query of that shape.
 *
 * How a caller reaches it: the tool discovers its host with
 * dynamic_cast<const KoCanvasPlatformHost *>(canvas) (SvgTextTool.cpp:310/829)
 * and dynamic_cast<const KoCanvasKeyBindingHost *>(canvas()) (:1174), i.e. the
 * cast target is the *canvas object itself*. This class is a plain object
 * holding a canvas pointer, so it is NOT reachable that way and must not be
 * read as one: for the tool to see these answers, the canvas-side host has to
 * derive from both faces and forward to an instance of this class (or derive
 * from it directly — KoCanvasPlatformHost / KoCanvasKeyBindingHost carry
 * defaults, so a canvas may also implement them itself).
 *
 * Wiring such a canvas-side host is not done in this tree today: no target
 * instantiates this class, exactly like the other three flake canvas host
 * faces (KoCanvasCursorHost / KoCanvasInputMethodHost / KoCanvasPlatformHost),
 * which have zero in-tree implementors. What is verified here is the answers'
 * equality with the live Qt platform, not that a tool currently receives them.
 */
class SvgTextQtPlatformHost final : public KoCanvasPlatformHost,
                                    public KoCanvasKeyBindingHost
{
public:
    explicit SvgTextQtPlatformHost(KoCanvasBase *canvas);

    int textCursorWidth() const override;
    int cursorFlashTime() const override;
    bool blinkCursorWhenTextSelected() const override;
    PkColor highlightColor() const override;

    TextCommand textCommand(int key, Pk::KeyboardModifiers modifiers) const override;
    PkKeySequence actionShortcut(const PkString &actionName) const override;

private:
    KoCanvasBase *m_canvas;
};

#endif // SVG_TEXT_QT_PLATFORM_HOST_H
