/*
 * SPDX-FileCopyrightText: 2026 Krita contributors
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef MYPAINTJSON_H
#define MYPAINTJSON_H

#include <string>
#include <utility>
#include <vector>

namespace MyPaintJson
{

using CurvePoint = std::pair<double, double>;

struct InputMapping
{
    std::string name;
    bool active = false;
    std::vector<CurvePoint> points;
};

std::string updateSetting(const std::string &source,
                          const std::string &settingName,
                          const std::vector<InputMapping> &inputs,
                          double baseValue);

} // namespace MyPaintJson

#endif // MYPAINTJSON_H
