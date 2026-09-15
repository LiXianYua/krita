/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SVG_TEXT_INPUT_METHOD_ADAPTER_H
#define SVG_TEXT_INPUT_METHOD_ADAPTER_H

#include <KisDocumentApplicationServices.h>
#include "SvgTextCursor.h"

class PkToolInputMethodEvent;


KisDocumentApplicationServices::InputMethodEvent
svgTextNativeInputMethodEvent(const PkToolInputMethodEvent &event);


#endif
