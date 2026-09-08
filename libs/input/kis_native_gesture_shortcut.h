/*
 *  SPDX-FileCopyrightText: 2017 Bernhard Liebl <poke1024@gmx.de>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 */

#ifndef KISNATIVEGESTURESHORTCUT_H
#define KISNATIVEGESTURESHORTCUT_H

#include "kis_abstract_shortcut.h"
#include <PkInputEvent.h>

class KRITAINPUT_EXPORT KisNativeGestureShortcut : public KisAbstractShortcut
{
public:
	KisNativeGestureShortcut(KisAbstractInputAction* action, int index, Pk::NativeGestureType type);
	~KisNativeGestureShortcut() override;

	int priority() const override;

	bool match(PkNativeGestureEvent* event);

private:
	class Private;
    Private * const d {nullptr};
};

#endif // KISNATIVEGESTURESHORTCUT_H
