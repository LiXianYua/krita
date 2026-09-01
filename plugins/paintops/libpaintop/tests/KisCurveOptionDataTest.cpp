/*
 *  SPDX-FileCopyrightText: 2022 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "KisCurveOptionDataTest.h"

#include <KisCurveOptionData.h>
#include <KisDynamicSensorIds.h>
#include <KisSensorData.h>
#include <KisStrokeSpeedMonitor.h>
#include <PkObject.h>
#include <PkXmlDocument.h>
#include <kis_properties_configuration.h>

void KisCurveOptionDataTest::testCurveOptionData()
{
    KisCurveOptionData data(KoID("Opacity"),
                            KisCurveOptionData::Checkability::NotCheckable);

    KisCurveOptionData data2 = data;

    QVERIFY(data == data2);

    data2.isChecked = false;

    QVERIFY(data != data2);

    data2 = data;

    QVERIFY(data == data2);

    data2.sensorStruct().sensorPressure.isActive = true;
    data2.sensorStruct().sensorPressure.curve = "0.0,0.5;1,1;";

    QVERIFY(data != data2);

    data2 = data;

    QVERIFY(data == data2);

    data2.sensorStruct().sensorPressure.isActive = true;
    data2.sensorStruct().sensorPressure.curve = "0.0,0.5;1,1;";

    KisPropertiesConfiguration config;

    data2.write(&config);

    QVERIFY(data != data2);

    QVERIFY(data.read(&config));

    QVERIFY(data == data2);

}

void KisCurveOptionDataTest::testSerializeDisabledSensors()
{
    KisCurveOptionData data(KoID("Opacity"),
                            KisCurveOptionData::Checkability::NotCheckable);

    // sensor is disabled!
    data.sensorStruct().sensorPressure.isActive = false;
    data.sensorStruct().sensorPressure.curve = "0.0,0.5;1,1;";

    data.sensorStruct().sensorRotation.isActive = true;
    data.sensorStruct().sensorRotation.curve = "0.0,0.5;1,1;";

    KisCurveOptionData data2(KoID("Opacity"),
                             KisCurveOptionData::Checkability::NotCheckable);

    data2 = data;
    QVERIFY(data == data2);

    data2 = KisCurveOptionData(KoID("Opacity"),
                               KisCurveOptionData::Checkability::NotCheckable);
    QVERIFY(data != data2);

    KisPropertiesConfiguration config;

    data.write(&config);
    QVERIFY(data2.read(&config));

    /**
     * The disabled sensor is **not** saved into the serialized
     * form, so the roundtripping such data will not result
     * in the same data in C++ sense.
     */

    QCOMPARE(data2.sensorStruct().sensorPressure.isActive, false);
    QCOMPARE(data2.sensorStruct().sensorRotation.isActive, true);

    QVERIFY(data != data2);
    data2.sensorStruct().sensorPressure.curve = data.sensorStruct().sensorPressure.curve;

    QVERIFY(data == data2);
}

void KisCurveOptionDataTest::testSerializeNoSensors()
{
    KisCurveOptionData data(KoID("Opacity"),
                            KisCurveOptionData::Checkability::NotCheckable);

    /**
     * When Krita loads a configuration with no sensors
     * available, it automatically activates a pressure
     * sensors with the default curve.
     */

    QCOMPARE(data.sensorStruct().sensorPressure.isActive, true);
    QCOMPARE(data.sensorStruct().sensorPressure.curve, DEFAULT_CURVE_STRING);
}

void KisCurveOptionDataTest::testLengthSensorTagRoundTrip()
{
    PkXmlDocument distanceDocument;
    PkXmlElement distanceElement = distanceDocument.createElement("sensor");
    distanceDocument.appendChild(distanceElement);

    KisSensorWithLengthData distance(DistanceId);
    distance.length = 73;
    distance.write(distanceDocument, distanceElement);

    const PkString distanceXml = distanceDocument.toString(-1);
    QVERIFY(distanceXml.contains(PkString(" length=\"73\"")));
    QVERIFY(!distanceXml.contains(PkString(" duration=")));

    KisSensorWithLengthData restoredDistance(DistanceId);
    restoredDistance.read(distanceElement);
    QCOMPARE(restoredDistance.length, 73);

    PkXmlDocument timeDocument;
    PkXmlElement timeElement = timeDocument.createElement("sensor");
    timeDocument.appendChild(timeElement);

    KisSensorWithLengthData time(TimeId, PkString("duration"));
    time.length = 41;
    time.write(timeDocument, timeElement);

    const PkString timeXml = timeDocument.toString(-1);
    QVERIFY(timeXml.contains(PkString(" duration=\"41\"")));
    QVERIFY(!timeXml.contains(PkString(" length=")));

    KisSensorWithLengthData restoredTime(TimeId, PkString("duration"));
    restoredTime.read(timeElement);
    QCOMPARE(restoredTime.length, 41);
}

void KisCurveOptionDataTest::testStrokeSpeedMonitorSignalLifetime()
{
    KisStrokeSpeedMonitor *monitor = KisStrokeSpeedMonitor::instance();
    const bool originalValue = monitor->haveStrokeSpeedMeasurement();
    int emissions = 0;

    {
        PkObject receiver;
        PkObject::connect(monitor, &KisStrokeSpeedMonitor::sigStatsUpdated,
                          &receiver, [&emissions] { ++emissions; });

        monitor->setHaveStrokeSpeedMeasurement(!originalValue);
        QCOMPARE(emissions, 1);
    }

    monitor->setHaveStrokeSpeedMeasurement(originalValue);
    QCOMPARE(emissions, 1);
}

SIMPLE_TEST_MAIN(KisCurveOptionDataTest)
