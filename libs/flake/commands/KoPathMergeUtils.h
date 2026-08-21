/*
 *  SPDX-FileCopyrightText: 2018 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KOPATHMERGEUTILS_H
#define KOPATHMERGEUTILS_H

#include <PkXmlCompat.h>

#include <boost/optional.hpp>

#include <pk/geometry/PkPoint.h>

class KoPathPoint;

namespace KritaUtils {

boost::optional<PkPointF> fetchControlPoint(KoPathPoint *pt, bool takeFirst);
void makeSymmetric(KoPathPoint *pt, bool copyFromFirst);
void restoreControlPoint(KoPathPoint *pt, bool restoreFirst, boost::optional<PkPointF> savedPoint);

}


#endif // KOPATHMERGEUTILS_H
