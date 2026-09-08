/*
 * SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef PSD_UNITS_H
#define PSD_UNITS_H

constexpr double psdPointsToInches(double points)
{
    return points * 0.01388888888889;
}

constexpr double psdInchesToPoints(double inches)
{
    return inches * 72.0;
}

constexpr double psdPointsToCentimeters(double points)
{
    return points * 0.0352777167;
}

#endif // PSD_UNITS_H
