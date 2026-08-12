/*
 * KDE. Krita Project.
 *
 * SPDX-FileCopyrightText: 2021 Deif Lou <ginoba@gmail.com>
 *
 * SPDX-License-Identifier: GPL-2.0-or-later
 */

#ifndef KIS_LEVELS_FILTER_CONFIGURATION_H
#define KIS_LEVELS_FILTER_CONFIGURATION_H

#include <optional>

#include <filter/kis_color_transformation_configuration.h>
#include <kis_paint_device.h>
#include <KisLevelsCurve.h>
#include <KisAutoLevels.h>

#include "../colorsfilters/virtual_channel_info.h"

class KisLevelsFilterConfiguration : public KisColorTransformationConfiguration
{
public:
    KisLevelsFilterConfiguration(int channelCount, qint32 version, KisResourcesInterfaceSP resourcesInterface);
    KisLevelsFilterConfiguration(int channelCount, KisResourcesInterfaceSP resourcesInterface);
    KisLevelsFilterConfiguration(const KisLevelsFilterConfiguration &rhs);

    KisFilterConfigurationSP clone() const override;

    static inline QString defaultName() { return "levels"; }
    static constexpr qint32 defaultVersion() { return 2; }
    static inline KisLevelsCurve defaultLevelsCurve() { return KisLevelsCurve(); }
    static constexpr bool defaultUseLightnessMode() { return true; }
    static constexpr bool defaultShowLogarithmicHistogram() { return false; }

    using KisFilterConfiguration::fromXML;
    using KisFilterConfiguration::toXML;
    using KisFilterConfiguration::fromLegacyXML;
    
    void fromLegacyXML(const QDomElement& root) override;
    void fromXML(const QDomElement& e) override;
    void toXML(QDomDocument& doc, QDomElement& root) const override;

    void setProperty(const QString &name, const QVariant &value) override;

    const QVector<KisLevelsCurve> levelsCurves() const;
    const KisLevelsCurve lightnessLevelsCurve() const;
    void setLevelsCurves(const QVector<KisLevelsCurve> &newLevelsCurves);
    void setLightnessLevelsCurve(const KisLevelsCurve &newLightnessLevelsCurve);
    const QVector<QVector<quint16>>& transfers() const;
    const QVector<quint16>& lightnessTransfer() const;

    bool useLightnessMode() const;
    bool showLogarithmicHistogram() const;
    void setUseLightnessMode(bool newUseLightnessMode);
    void setShowLogarithmicHistogram(bool newShowLogarithmicHistogram);

    bool isCompatible(const KisPaintDeviceSP) const override;

    void setDefaults();

    struct AutoLevelsDefaults
    {
        std::optional<qreal> maximumInputBlackAndWhiteOffset;
        std::optional<KisAutoLevels::MidtonesAdjustmentMethod> midtonesAdjustmentMethod;
        std::optional<qreal> midtonesAdjustmentAmount;
    };
    /// 自动色阶的经验参数（摘自 D-02-b 删除的 KisLevelsConfigWidget）
    /// 上游原注释：These were selected empirically, there is no strong reason why they should be like this
    /// 字段为 std::optional：某个字段是 nullopt 表示原代码在对应分支下根本不调用
    /// 对应的 setter，调用方应保留自己的既有值，不是被这个函数强行覆盖成某个值。
    static AutoLevelsDefaults autoLevelsDefaults(const KoColorSpace *cs, bool lightnessMode, const VirtualChannelInfo &channel);

private:
    QVector<QVector<quint16>> m_transfers;
    QVector<quint16> m_lightnessTransfer;

    int channelCount() const;
    void setChannelCount(int newChannelCount);

    void setLightessLevelsCurveFromLegacyValues();
    void setLegacyValuesFromLightnessLevelsCurve();

    void updateTransfers();
    void updateLightnessTransfer();
};

#endif
