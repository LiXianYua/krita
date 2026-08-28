/*
 *  SPDX-FileCopyrightText: 2022 Agata Cacko <cacko.azh@gmail.com>
 *  SPDX-FileCopyrightText: 2011 Lukáš Tvrdý <lukast.dev@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisCurveOpOptionData.h"

#include "kis_properties_configuration.h"

// new rewrite
const PkString CURVE_LINE_WIDTH = "Curve/lineWidth"; // same as in sketch
const PkString CURVE_PAINT_CONNECTION_LINE = "Curve/makeConnection"; // same as in sketch
const PkString CURVE_STROKE_HISTORY_SIZE = "Curve/strokeHistorySize";
const PkString CURVE_SMOOTHING = "Curve/smoothing";
const PkString CURVE_CURVES_OPACITY = "Curve/curvesOpacity";


bool KisCurveOpOptionData::read(const KisPropertiesConfiguration *config)
{
	curve_paint_connection_line = config->getBool(CURVE_PAINT_CONNECTION_LINE);
	curve_smoothing = config->getBool(CURVE_SMOOTHING);
	curve_stroke_history_size = config->getInt(CURVE_STROKE_HISTORY_SIZE);
	curve_line_width = config->getInt(CURVE_LINE_WIDTH);
	curve_curves_opacity = config->getDouble(CURVE_CURVES_OPACITY);

    return true;
}

void KisCurveOpOptionData::write(KisPropertiesConfiguration *config) const
{
    config->setProperty(CURVE_PAINT_CONNECTION_LINE, curve_paint_connection_line);
	config->setProperty(CURVE_SMOOTHING, curve_smoothing);
	config->setProperty(CURVE_STROKE_HISTORY_SIZE, curve_stroke_history_size);
	config->setProperty(CURVE_LINE_WIDTH, curve_line_width);
	config->setProperty(CURVE_CURVES_OPACITY, curve_curves_opacity);
}
