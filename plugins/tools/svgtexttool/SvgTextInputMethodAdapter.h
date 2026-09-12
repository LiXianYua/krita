/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SVG_TEXT_INPUT_METHOD_ADAPTER_H
#define SVG_TEXT_INPUT_METHOD_ADAPTER_H

#include <KisDocumentApplicationServices.h>
#include "SvgTextCursor.h"

class QInputMethodEvent;
class QKeyEvent;
class PkToolInputMethodEvent;

KisDocumentApplicationServices::InputMethodEvent
svgTextNativeInputMethodEvent(const QInputMethodEvent &event);

KisDocumentApplicationServices::InputMethodEvent
svgTextNativeInputMethodEvent(const PkToolInputMethodEvent &event);

SvgTextCursor::NativeKeyEvent
svgTextNativeKeyEvent(const QKeyEvent &event,
                      KoSvgText::WritingMode writingMode,
                      KoSvgText::Direction direction,
                      const KoCanvasKeyBindingHost *bindingHost = nullptr);

#endif
