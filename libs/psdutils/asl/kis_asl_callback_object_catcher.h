/*
 *  SPDX-FileCopyrightText: 2015 Dmitry Kazakov <dimula73@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef __KIS_ASL_CALLBACK_OBJECT_CATCHER_H
#define __KIS_ASL_CALLBACK_OBJECT_CATCHER_H

#include "kis_asl_object_catcher.h"

#include <PkAuxTypes.h>
#include <PkPointer.h>
#include <PkScopedPointer.h>
#include <functional>

#include <resources/KoAbstractGradient.h>

#include "kritapsdutils_export.h"

class KoPattern;

using ASLCallbackDouble = std::function<void(double)>;
using ASLCallbackInteger = std::function<void(int)>;
using ASLCallbackString = std::function<void(const PkString &)>;
using ASLCallbackBoolean = std::function<void(bool)>;
using ASLCallbackColor = std::function<void(const KoColor &)>;
using ASLCallbackPoint = std::function<void(const PkPointF &)>;
using ASLCallbackCurve = std::function<void(const PkString &, const PkVector<PkPointF> &)>;
using ASLCallbackPattern = std::function<void(const KoPatternSP, const PkString &)>;
using ASLCallbackPatternRef = std::function<void(const PkString &, const PkString &)>;
using ASLCallbackGradient = std::function<void(KoAbstractGradientSP)>;
using ASLCallbackNewStyle = std::function<void()>;
using ASLCallbackRawData = std::function<void(PkByteArray)>;
using ASLCallbackTransform = std::function<void(PkTransform)>;
using ASLCallbackRect = std::function<void(PkRectF)>;

class KRITAPSDUTILS_EXPORT KisAslCallbackObjectCatcher : public KisAslObjectCatcher
{
public:
    KisAslCallbackObjectCatcher();
    ~KisAslCallbackObjectCatcher() override;

    void addDouble(const PkString &path, double value) override;
    void addInteger(const PkString &path, int value) override;
    void addEnum(const PkString &path, const PkString &typeId, const PkString &value) override;
    void addUnitFloat(const PkString &path, const PkString &unit, double value) override;
    void addText(const PkString &path, const PkString &value) override;
    void addBoolean(const PkString &path, bool value) override;
    void addColor(const PkString &path, const KoColor &value) override;
    void addPoint(const PkString &path, const PkPointF &value) override;
    void addCurve(const PkString &path, const PkString &name, const PkVector<PkPointF> &points) override;
    void addPattern(const PkString &path, const KoPatternSP pattern, const PkString &patternUuid) override;
    void addPatternRef(const PkString &path, const PkString &patternUuid, const PkString &patternName) override;
    void addGradient(const PkString &path, KoAbstractGradientSP gradient) override;
    void newStyleStarted() override;
    void addRawData(const PkString &path, PkByteArray ba) override;
    void addTransform(const PkString &path, const PkTransform &transform) override;
    void addRect(const PkString &path, const PkRectF &rect) override;
    void addUnitRect(const PkString &path, const PkString &unit, const PkRectF &rect) override;


    void subscribeDouble(const PkString &path, ASLCallbackDouble callback);
    void subscribeInteger(const PkString &path, ASLCallbackInteger callback);
    void subscribeEnum(const PkString &path, const PkString &typeId, ASLCallbackString callback);
    void subscribeUnitFloat(const PkString &path, const PkString &unit, ASLCallbackDouble callback);
    void subscribeText(const PkString &path, ASLCallbackString callback);
    void subscribeBoolean(const PkString &path, ASLCallbackBoolean callback);
    void subscribeColor(const PkString &path, ASLCallbackColor callback);
    void subscribePoint(const PkString &path, ASLCallbackPoint callback);
    void subscribeCurve(const PkString &path, ASLCallbackCurve callback);
    void subscribePattern(const PkString &path, ASLCallbackPattern callback);
    void subscribePatternRef(const PkString &path, ASLCallbackPatternRef callback);
    void subscribeGradient(const PkString &path, ASLCallbackGradient callback);
    void subscribeNewStyleStarted(ASLCallbackNewStyle callback);
    void subscribeRawData(const PkString &path, ASLCallbackRawData callback);
    void subscribeTransform(const PkString &path, ASLCallbackTransform callback);
    void subscribeRect(const PkString &path, ASLCallbackRect callback);
    void subscribeUnitRect(const PkString &path, const PkString &unit, ASLCallbackRect callback);

private:
    struct Private;
    const PkScopedPointer<Private> m_d;
};

#endif /* __KIS_ASL_CALLBACK_OBJECT_CATCHER_H */
