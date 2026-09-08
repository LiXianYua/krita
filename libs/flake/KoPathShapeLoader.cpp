/* This file is part of the KDE project
 * SPDX-FileCopyrightText: 2007 Jan Hambrecht <jaham@gmx.net>
 *
 * SPDX-License-Identifier: LGPL-2.0-or-later
 */

#include "KoPathShapeLoader.h"
#include "KoPathShape.h"
#include <PkSvgPathParser_p.h>

class KoPathShapeLoaderPrivate : public PkSvgPathParser<KoPathShape>
{
public:
    using PkSvgPathParser<KoPathShape>::PkSvgPathParser;
};

KoPathShapeLoader::KoPathShapeLoader(KoPathShape *path)
    : d(new KoPathShapeLoaderPrivate(path))
{
}

KoPathShapeLoader::~KoPathShapeLoader()
{
    delete d;
}

void KoPathShapeLoader::parseSvg(const PkString &s, bool process)
{
    d->parseSvg(s, process);
}
