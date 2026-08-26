/*
 *  SPDX-FileCopyrightText: 2023 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#ifndef KISDYNAMICSENSORIDS_H
#define KISDYNAMICSENSORIDS_H

#include <KoID.h>

const KoID FuzzyPerDabId("fuzzy", PkString("Fuzzy Dab")); ///< generate a random number
const KoID FuzzyPerStrokeId("fuzzystroke", PkString("Fuzzy Stroke")); ///< generate a random number
const KoID SpeedId("speed", PkString("Speed")); ///< generate a number depending on the speed of the cursor
const KoID FadeId("fade", PkString("Fade")); ///< generate a number that increase every time you call it (e.g. per dab)
const KoID DistanceId("distance", PkString("Distance")); ///< generate a number that increase with distance
const KoID TimeId("time", PkString("Time")); ///< generate a number that increase with time
const KoID DrawingAngleId("drawingangle", PkString("Drawing angle")); ///< number depending on the angle
const KoID RotationId("rotation", PkString("Rotation")); ///< rotation coming from the device
const KoID PressureId("pressure", PkString("Pressure")); ///< number depending on the pressure
const KoID PressureInId("pressurein", PkString("PressureIn")); ///< number depending on the pressure
const KoID XTiltId("xtilt", PkString("X-Tilt")); ///< number depending on X-tilt
const KoID YTiltId("ytilt", PkString("Y-Tilt")); ///< number depending on Y-tilt

/**
 * "TiltDirection" and "TiltElevation" parameters are written to
 * preset files as "ascension" and "declination" to keep backward
 * compatibility with older presets from the days when they were called
 * differently.
 */
const KoID TiltDirectionId("ascension", PkString("Tilt direction")); /// < number depending on the X and Y tilt, tilt direction is 0 when stylus nib points to you and changes clockwise from -180 to +180.
const KoID TiltElevationId("declination", PkString("Tilt elevation")); /// < tilt elevation is 90 when stylus is perpendicular to tablet and 0 when it's parallel to tablet

const KoID PerspectiveId("perspective", PkString("Perspective")); ///< number depending on the distance on the perspective grid
const KoID TangentialPressureId("tangentialpressure", PkString("Tangential pressure")); ///< the wheel on an airbrush device
const KoID SensorsListId("sensorslist", "SHOULD NOT APPEAR IN THE UI !"); ///< this a non user-visible sensor that can store a list of other sensors, and multiply their output


#endif // KISDYNAMICSENSORIDS_H
