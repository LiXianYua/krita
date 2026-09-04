/*
 *  SPDX-FileCopyrightText: 2016 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOFLAKETYPES_H
#define KOFLAKETYPES_H

class KoShapeStroke;
class KoShapeStrokeModel;

template<class T> class PkSharedPointer;

typedef PkSharedPointer<KoShapeStrokeModel> KoShapeStrokeModelSP;
typedef PkSharedPointer<KoShapeStroke> KoShapeStrokeSP;

#endif // KOFLAKETYPES_H

