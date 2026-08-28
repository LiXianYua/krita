/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "ora_save_policy.h"

bool oraLayerPayloadSucceeded(const PkString &path)
{
    return !path.isEmpty();
}

void oraAppendStackChild(PkXmlNode parent, const PkXmlNode &child)
{
    parent.insertBefore(child, PkXmlNode());
}
