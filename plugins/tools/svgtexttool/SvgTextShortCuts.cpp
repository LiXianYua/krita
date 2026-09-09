/*
 *  SPDX-FileCopyrightText: 2025 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */
#include "SvgTextShortCuts.h"
#include <KoSvgTextProperties.h>

/**
 * @brief The SvgTextShortcutInfo class
 * This is a struct that describes a text property shortcut.
 */
struct SvgTextShortcutInfo : public boost::equality_comparable<SvgTextShortcutInfo> {

    enum ActionType {
        Set, ///< Will set value1, cannot be toggled.
        Toggle, ///< Toggle will test "testValue", and toggle between value1 and value 2;
        Increase, //< Will increase by value 1.
        Decrease //< Will decrease by value 1.
    };
    SvgTextShortcutInfo() {}

    static SvgTextShortcutInfo propertyToggle(KoSvgTextProperties::PropertyId _propertyId,
                                              PkVariant _value1, PkVariant _value2, PkVariant _testValue) {
        SvgTextShortcutInfo info;
        info.propertyId = _propertyId;
        info.type = Toggle;
        info.value1 = _value1;
        info.value2 = _value2;
        info.testValue = _testValue;
        return info;
    }

    static SvgTextShortcutInfo propertyChange(KoSvgTextProperties::PropertyId _propertyId, PkVariant _value1, bool _increase) {
        SvgTextShortcutInfo info;
        info.propertyId = _propertyId;
        info.type = _increase? Increase: Decrease;
        info.value1 = _value1;
        return info;
    }

    static SvgTextShortcutInfo propertySet(KoSvgTextProperties::PropertyId _propertyId, PkVariant _value1) {
        SvgTextShortcutInfo info;
        info.propertyId = _propertyId;
        info.type = Set;
        info.value1 = _value1;
        return info;
    }

    KoSvgTextProperties::PropertyId propertyId;
    ActionType type;
    PkVariant value1;
    PkVariant value2;
    PkVariant testValue;



    bool operator==(const SvgTextShortcutInfo & other) const {
        return (propertyId == other.propertyId
                && type == other.type
                && value1 == other.value1
                && value2 == other.value2
                && testValue == other.testValue);
    }
};

const PkMap<PkString, SvgTextShortcutInfo> textShortCuts = {
    {
        "svg_weight_bold",
        SvgTextShortcutInfo::propertyToggle(KoSvgTextProperties::FontWeightId,
        PkVariant(400), PkVariant(700), PkVariant(500))
    },

    {
        "svg_weight_normal",
        SvgTextShortcutInfo::propertySet(KoSvgTextProperties::FontWeightId,
        PkVariant(400))
    },
    {
        "svg_weight_demi",
        SvgTextShortcutInfo::propertySet(KoSvgTextProperties::FontWeightId,
        PkVariant(600))
    },
    {
        "svg_weight_black",
        SvgTextShortcutInfo::propertySet(KoSvgTextProperties::FontWeightId,
        PkVariant(900))
    },
    {
        "svg_weight_light",
        SvgTextShortcutInfo::propertySet(KoSvgTextProperties::FontWeightId,
        PkVariant(300))
    },

    {
        "svg_format_italic",
        SvgTextShortcutInfo::propertyToggle(KoSvgTextProperties::FontStyleId,
        PkVariant::fromValue(KoSvgText::CssFontStyleData(PkFontStyleNormal)),
        PkVariant::fromValue(KoSvgText::CssFontStyleData(PkFontStyleItalic)),
        PkVariant::fromValue(KoSvgText::CssFontStyleData(PkFontStyleNormal)))
    },

    {
        "svg_increase_font_size",
        SvgTextShortcutInfo::propertyChange(KoSvgTextProperties::FontSizeId,
        PkVariant::fromValue(1), true)
    },

    {
        "svg_decrease_font_size",
        SvgTextShortcutInfo::propertyChange(KoSvgTextProperties::FontSizeId,
        PkVariant::fromValue(1), false)
    },

    {
        "svg_format_underline",
        SvgTextShortcutInfo::propertyToggle(KoSvgTextProperties::TextDecorationLineId,
        PkVariant(true), PkVariant(false), PkVariant(KoSvgText::DecorationUnderline))
    },
    {
        "svg_format_strike_through",
        SvgTextShortcutInfo::propertyToggle(KoSvgTextProperties::TextDecorationLineId,
        PkVariant(true), PkVariant(false), PkVariant(KoSvgText::DecorationLineThrough))
    },
    {
        "svg_font_kerning",
        SvgTextShortcutInfo::propertyToggle(KoSvgTextProperties::KerningId,
        PkVariant(true), PkVariant(false), PkVariant(true))
    },
    {
        "svg_align_right",
        SvgTextShortcutInfo::propertySet(KoSvgTextProperties::TextAlignAllId,
        PkVariant(KoSvgText::AlignStart))
    },
    {
        "svg_align_left",
        SvgTextShortcutInfo::propertySet(KoSvgTextProperties::TextAlignAllId,
        PkVariant(KoSvgText::AlignEnd))
    },
    {
        "svg_align_center",
        SvgTextShortcutInfo::propertySet(KoSvgTextProperties::TextAlignAllId,
        PkVariant(KoSvgText::AlignCenter))
    },
    {
        "svg_align_justified",
        SvgTextShortcutInfo::propertySet(KoSvgTextProperties::TextAlignAllId,
        PkVariant(KoSvgText::AlignJustify))
    },
    {
        "svg_format_subscript",
        SvgTextShortcutInfo::propertySet(KoSvgTextProperties::BaselineShiftModeId,
        PkVariant(KoSvgText::ShiftSub))
    },
    {
        "svg_format_superscript",
        SvgTextShortcutInfo::propertySet(KoSvgTextProperties::BaselineShiftModeId,
        PkVariant(KoSvgText::ShiftSuper))
    }
};

PkStringList SvgTextShortCuts::possibleActions()
{
    return textShortCuts.keys();
}

bool SvgTextShortCuts::isAction(const PkString &name)
{
    return textShortCuts.contains(name);
}
/**
 * @brief testPropertyEnabled
 * @param info
 * @param currentProperties
 * @return whether any properties in the current properties pass the info.testValue.
 */
bool testPropertyEnabled(const SvgTextShortcutInfo &info, const PkList<KoSvgTextProperties> currentProperties)
{
    const PkVariant testValue = info.type == SvgTextShortcutInfo::Toggle? info.testValue: info.value1;

    for (auto properties = currentProperties.begin(); properties != currentProperties.end(); properties++) {
        const PkVariant oldValue = properties->propertyOrDefault(info.propertyId);


        if (oldValue.canConvert<KoSvgText::TextDecorations>() && info.propertyId == KoSvgTextProperties::TextDecorationLineId) {

            const KoSvgText::TextDecorations oldDecor = oldValue.value<KoSvgText::TextDecorations>();
            return oldDecor.testFlag(KoSvgText::TextDecoration(testValue.toInt()));

        } else if (oldValue.canConvert<KoSvgText::CssFontStyleData>()) {
            const KoSvgText::CssFontStyleData testVal = testValue.value<KoSvgText::CssFontStyleData>();
            const KoSvgText::CssFontStyleData currentVal = oldValue.value<KoSvgText::CssFontStyleData>();
            return (testVal.style != currentVal.style);
        } else if (oldValue.canConvert<KoSvgText::AutoValue>()) {
            const KoSvgText::AutoValue currentVal = oldValue.value<KoSvgText::AutoValue>();
            if (testValue.canConvert<KoSvgText::AutoValue>()) {
                return (testValue == oldValue);
            } else if (testValue.canConvert<double>()) {
                return currentVal.customValue == testValue.toDouble();
            } else {
                return currentVal.isAuto == testValue.toBool();
            }
        }  else if (oldValue.canConvert<KoSvgText::CssLengthPercentage>()) {
            const KoSvgText::CssLengthPercentage currentVal = oldValue.value<KoSvgText::CssLengthPercentage>();
            if (testValue.canConvert<KoSvgText::CssLengthPercentage>()) {
                return (testValue == oldValue);
            } else {
                return currentVal.value >= testValue.toDouble();
            }
        } else if (oldValue.canConvert<KoSvgText::AutoLengthPercentage>()) {
            const KoSvgText::AutoLengthPercentage currentVal = oldValue.value<KoSvgText::CssLengthPercentage>();
            if (testValue.canConvert<KoSvgText::AutoLengthPercentage>()) {
                return (testValue == oldValue);
            } else if (testValue.canConvert<KoSvgText::CssLengthPercentage>()) {
                return (testValue == PkVariant::fromValue(currentVal.length));
            } else {
                return currentVal.length.value == testValue.toDouble();
            }
        } else if (oldValue.canConvert<KoSvgText::LineHeightInfo>()) {
            const KoSvgText::LineHeightInfo currentVal = oldValue.value<KoSvgText::LineHeightInfo>();
            if (testValue.canConvert<KoSvgText::LineHeightInfo>()) {
                return (testValue == oldValue);
            } else if (testValue.canConvert<KoSvgText::CssLengthPercentage>()) {
                return (testValue == PkVariant::fromValue(currentVal.length));
            } else {
                return currentVal.length.value == testValue.toDouble();
            }
        }
        return (testValue.toDouble() <= oldValue.toDouble());
    }
    return false;
}

bool SvgTextShortCuts::actionEnabled(const PkString &name, const PkList<KoSvgTextProperties> currentProperties) {
    if (!textShortCuts.contains(name)) return false;
    const SvgTextShortcutInfo info = textShortCuts.value(name);

    if (info.type != SvgTextShortcutInfo::Toggle && info.type != SvgTextShortcutInfo::Set) {
        return false;
    }

    return testPropertyEnabled(info, currentProperties);
}

/**
 * @brief toggleProperty
 * Handles toggling properties for getModifiedProperties
 * split out to make code easier to navigate.
 */
PkVariant toggleProperty(SvgTextShortcutInfo info, bool checked, PkList<KoSvgTextProperties> currentProperties) {
    PkVariant newVal;

    if (currentProperties.isEmpty()) return newVal;

    PkVariant oldValue = currentProperties.first().propertyOrDefault(info.propertyId);
    if (oldValue.canConvert<KoSvgText::TextDecorations>() && info.propertyId == KoSvgTextProperties::TextDecorationLineId) {
        KoSvgText::TextDecoration decor = KoSvgText::TextDecoration(info.testValue.toInt());
        KoSvgText::TextDecorations newDecor;
        newDecor.setFlag(decor, checked);
        newVal = PkVariant::fromValue(newDecor);

    } else {
        if (checked) {
            newVal = info.value2;
        } else {
            newVal = info.value1;
        }
    }

    return newVal;
}

/**
 * @brief adjustValue
 * Handles increase/decrease value for getModifiedProperties,
 * split out to make code easier to navigate.
 *
 * TODO: handle max/min.
 */
PkVariant adjustValue(SvgTextShortcutInfo info, PkVariant oldValue) {
    PkVariant newVal;

    if (oldValue.canConvert<KoSvgText::CssLengthPercentage>()) {
        KoSvgText::CssLengthPercentage length = oldValue.value<KoSvgText::CssLengthPercentage>();
        if (info.type == SvgTextShortcutInfo::Increase) {
            length.value += info.value1.toDouble();
        } else {
            length.value -= info.value1.toDouble();
        }
        newVal = PkVariant::fromValue(length);
    } else if (oldValue.canConvert<KoSvgText::AutoLengthPercentage>()) {
        KoSvgText::AutoLengthPercentage length = oldValue.value<KoSvgText::AutoLengthPercentage>();
        length.isAuto = false;
        if (info.type == SvgTextShortcutInfo::Increase) {
            length.length.value += info.value1.toDouble();
        } else {
            length.length.value -= info.value1.toDouble();
        }
        newVal = PkVariant::fromValue(length);
    } else if (oldValue.canConvert<KoSvgText::AutoValue>()) {
        KoSvgText::AutoValue value = oldValue.value<KoSvgText::AutoValue>();
        value.isAuto = false;
        if (info.type == SvgTextShortcutInfo::Increase) {
            value.customValue += info.value1.toDouble();
        } else {
            value.customValue -= info.value1.toDouble();
        }
        newVal = PkVariant::fromValue(value);
    } else if (oldValue.canConvert<double>()) {
        double value = oldValue.toDouble();
        if (info.type == SvgTextShortcutInfo::Increase) {
            value += info.value1.toDouble();
        } else {
            value -= info.value1.toDouble();
        }
        newVal = PkVariant::fromValue(value);
    } else if (oldValue.canConvert<int>()) {
        int value = oldValue.toInt();
        if (info.type == SvgTextShortcutInfo::Increase) {
            value += info.value1.toInt();
        } else {
            value -= info.value1.toInt();
        }
        newVal = PkVariant::fromValue(value);
    }

    return newVal;
}

KoSvgTextProperties SvgTextShortCuts::getModifiedProperties(const PkString &name,
                                                            bool checked,
                                                            PkList<KoSvgTextProperties> currentProperties)
{
    if (currentProperties.isEmpty() || !textShortCuts.contains(name)) return KoSvgTextProperties();
    const SvgTextShortcutInfo info = textShortCuts.value(name);

    PkVariant newVal;
    if (info.type == SvgTextShortcutInfo::Toggle) {
        newVal = toggleProperty(info, checked, currentProperties);
    } else if (info.type == SvgTextShortcutInfo::Set) {
        KoSvgTextProperties properties = currentProperties.first();
        PkVariant oldValue = properties.propertyOrDefault(info.propertyId);

        if (oldValue.canConvert<int>()) {
            newVal = info.value1;
        }
    } else {
        KoSvgTextProperties properties = currentProperties.first();
        PkVariant oldValue = properties.propertyOrDefault(info.propertyId);

        newVal = adjustValue(info, oldValue);
    }
    KoSvgTextProperties newProperties;
    newProperties.setProperty(info.propertyId, newVal);
    return newProperties;
}
