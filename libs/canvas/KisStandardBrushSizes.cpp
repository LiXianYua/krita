/*
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "KisStandardBrushSizes.h"

#include <algorithm>
#include <cmath>
#include <functional>

KisStandardBrushSizes::KisStandardBrushSizes(int minSize, int maxSize)
{
    int brushSize = minSize;
    do {
        m_sizes.push_back(brushSize);
        const int increment = qMax(1, int(std::ceil(qreal(brushSize) / 15)));
        brushSize += increment;
    } while (brushSize < maxSize);
    m_sizes.push_back(maxSize);
}

int KisStandardBrushSizes::increaseBrushSize(qreal size)
{
    const auto result = std::upper_bound(m_sizes.begin(), m_sizes.end(), qRound(size));
    return result != m_sizes.end() ? *result : m_sizes.back();
}

int KisStandardBrushSizes::decreaseBrushSize(qreal size)
{
    const auto result = std::upper_bound(m_sizes.rbegin(), m_sizes.rend(), qRound(size), std::greater<int>());
    return result != m_sizes.rend() ? *result : m_sizes.front();
}
