/*
 *  SPDX-FileCopyrightText: 2024 Halla Rempt <halla@valdyas.org>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISQSTRINGLISTFWD_H
#define KISQSTRINGLISTFWD_H

#include <cstdint>
#include <algorithm>
#include <cmath>

#if (QT_VERSION < QT_VERSION_CHECK(6, 0, 0))
class PkStringList;
#else
class PkString;
class PkByteArray;
template <typename T> class PkList;
template<typename T> using PkVector = PkList<T>;
using PkStringList = PkList<PkString>;
using PkByteArrayList = PkList<PkByteArray>;
#endif

#endif // KISQSTRINGLISTFWD_H
