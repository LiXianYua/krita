/*
 * SPDX-FileCopyrightText: 2026 S-09-c
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef SPRITER_FORMAT_H
#define SPRITER_FORMAT_H

class PkStream;
class PkXmlDocument;

bool writeSpriterScml(PkStream *device, const PkXmlDocument &document);

#endif
