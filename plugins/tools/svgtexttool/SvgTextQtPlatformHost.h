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
 * side holds no Qt query of that shape. Owned by the Qt host that owns the
 * canvas; the canvas may hand itself (or a wrapper) to a tool via dynamic_cast.
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
