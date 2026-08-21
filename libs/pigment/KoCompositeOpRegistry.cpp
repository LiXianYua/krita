/*
 *  SPDX-FileCopyrightText: 2005 Adrian Page <adrian@pagenet.plus.com>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/

#include <PkXmlCompat.h>

#include "KoCompositeOpRegistry.h"



#include <KoID.h>
#include "KoCompositeOp.h"
#include "KoColorSpace.h"

#include "kis_assert.h"
#include "DebugPigment.h"

namespace {
// std::multimap 迭代器解引用得 std::pair<const KoID,KoID>（不是 value），
// 原多值映射的 std::find(m_map, KoID) 按值查找不成立——改按 value 线性查找。
std::multimap<KoID,KoID>::const_iterator
findCompositeOp(const std::multimap<KoID,KoID>& map, const KoID& id)
{
    for (auto it = map.cbegin(); it != map.cend(); ++it) {
        if (it->second == id) {
            return it;
        }
    }
    return map.cend();
}

} // namespace

static KoID koidCompositeOverStatic() {
    static const KoID compositeOver(COMPOSITE_OVER, PkString("Normal"));
    return compositeOver;
}

KoCompositeOpRegistry::KoCompositeOpRegistry()
{
    m_categories
        << KoID("arithmetic",  PkString("Arithmetic"))
        << KoID("binary"    ,  PkString("Binary"))
        << KoID("dark"      ,  PkString("Darken"))
        << KoID("light"     ,  PkString("Lighten"))
        << KoID("modulo"       ,  PkString("Modulo"))
        << KoID("negative"  ,  PkString("Negative"))
        << KoID("mix"       ,  PkString("Mix"))
        << KoID("misc"      ,  PkString("Misc"))
        << KoID("hsy"       ,  PkString("HSY"))
        << KoID("hsi"       ,  PkString("HSI"))
        << KoID("hsl"       ,  PkString("HSL"))
        << KoID("hsv"       ,  PkString("HSV"))
        << KoID("quadratic" ,  PkString("Quadratic"));

    m_map.emplace(m_categories[0], KoID(COMPOSITE_ADD             ,  PkString("Addition")));
    m_map.emplace(m_categories[0], KoID(COMPOSITE_SUBTRACT        ,  PkString("Subtract")));
    m_map.emplace(m_categories[0], KoID(COMPOSITE_MULT            ,  PkString("Multiply")));
    m_map.emplace(m_categories[0], KoID(COMPOSITE_DIVIDE          ,  PkString("Divide")));
    m_map.emplace(m_categories[0], KoID(COMPOSITE_INVERSE_SUBTRACT,  PkString("Inverse Subtract")));
    
    m_map.emplace(m_categories[1], KoID(COMPOSITE_XOR             ,  PkString("XOR")));
    m_map.emplace(m_categories[1], KoID(COMPOSITE_OR              ,  PkString("OR")));
    m_map.emplace(m_categories[1], KoID(COMPOSITE_AND             ,  PkString("AND")));
    m_map.emplace(m_categories[1], KoID(COMPOSITE_NAND            ,  PkString("NAND")));
    m_map.emplace(m_categories[1], KoID(COMPOSITE_NOR             ,  PkString("NOR")));
    m_map.emplace(m_categories[1], KoID(COMPOSITE_XNOR            ,  PkString("XNOR")));
    m_map.emplace(m_categories[1], KoID(COMPOSITE_IMPLICATION     ,  PkString("IMPLICATION")));
    m_map.emplace(m_categories[1], KoID(COMPOSITE_NOT_IMPLICATION ,  PkString("NOT IMPLICATION")));
    m_map.emplace(m_categories[1], KoID(COMPOSITE_CONVERSE        ,  PkString("CONVERSE")));
    m_map.emplace(m_categories[1], KoID(COMPOSITE_NOT_CONVERSE    ,  PkString("NOT CONVERSE")));

    m_map.emplace(m_categories[2], KoID(COMPOSITE_BURN       ,  PkString("Burn")));
    m_map.emplace(m_categories[2], KoID(COMPOSITE_LINEAR_BURN,  PkString("Linear Burn")));
    m_map.emplace(m_categories[2], KoID(COMPOSITE_DARKEN     ,  PkString("Darken")));
    m_map.emplace(m_categories[2], KoID(COMPOSITE_GAMMA_DARK ,  PkString("Gamma Dark")));
    m_map.emplace(m_categories[2], KoID(COMPOSITE_DARKER_COLOR     ,  PkString("Darker Color")));
    m_map.emplace(m_categories[2], KoID(COMPOSITE_SHADE_IFS_ILLUSIONS,  PkString("Shade (IFS Illusions)")));
    m_map.emplace(m_categories[2], KoID(COMPOSITE_FOG_DARKEN_IFS_ILLUSIONS,  PkString("Fog Darken (IFS Illusions)")));
    m_map.emplace(m_categories[2], KoID(COMPOSITE_EASY_BURN       ,  PkString("Easy Burn")));

    m_map.emplace(m_categories[3], KoID(COMPOSITE_DODGE       ,  PkString("Color Dodge")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_DODGE_HDR   ,  PkString("Color Dodge HDR")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_LINEAR_DODGE,  PkString("Linear Dodge")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_LIGHTEN     ,  PkString("Lighten")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_LINEAR_LIGHT,  PkString("Linear Light")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_SCREEN      ,  PkString("Screen")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_PIN_LIGHT   ,  PkString("Pin Light")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_VIVID_LIGHT ,  PkString("Vivid Light")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_VIVID_LIGHT_HDR , PkString("Vivid Light HDR")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_FLAT_LIGHT  ,  PkString("Flat Light")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_HARD_LIGHT  ,  PkString("Hard Light")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_SOFT_LIGHT_IFS_ILLUSIONS,  PkString("Soft Light (IFS Illusions)")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_SOFT_LIGHT_PEGTOP_DELPHI,  PkString("Soft Light (Pegtop-Delphi)")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_SOFT_LIGHT_PHOTOSHOP,  PkString("Soft Light (Photoshop)")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_SOFT_LIGHT_SVG,  PkString("Soft Light (SVG)")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_GAMMA_LIGHT ,  PkString("Gamma Light")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_GAMMA_ILLUMINATION ,  PkString("Gamma Illumination")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_LIGHTER_COLOR     ,  PkString("Lighter Color")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_PNORM_A           ,  PkString("P-Norm A")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_PNORM_B           ,  PkString("P-Norm B")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_SUPER_LIGHT     ,  PkString("Super Light")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_TINT_IFS_ILLUSIONS,  PkString("Tint (IFS Illusions)")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_FOG_LIGHTEN_IFS_ILLUSIONS,  PkString("Fog Lighten (IFS Illusions)")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_EASY_DODGE       ,  PkString("Easy Dodge")));
    m_map.emplace(m_categories[3], KoID(COMPOSITE_LUMINOSITY_SAI       ,  PkString("Luminosity/Shine (SAI)")));

    m_map.emplace(m_categories[4], KoID(COMPOSITE_MOD              ,  PkString("Modulo")));
    m_map.emplace(m_categories[4], KoID(COMPOSITE_MOD_CON          ,  PkString("Modulo - Continuous")));
    m_map.emplace(m_categories[4], KoID(COMPOSITE_DIVISIVE_MOD     ,  PkString("Divisive Modulo")));
    m_map.emplace(m_categories[4], KoID(COMPOSITE_DIVISIVE_MOD_CON ,  PkString("Divisive Modulo - Continuous")));
    m_map.emplace(m_categories[4], KoID(COMPOSITE_MODULO_SHIFT     ,  PkString("Modulo Shift")));
    m_map.emplace(m_categories[4], KoID(COMPOSITE_MODULO_SHIFT_CON ,  PkString("Modulo Shift - Continuous")));

    m_map.emplace(m_categories[5], KoID(COMPOSITE_DIFF                 ,  PkString("Difference")));
    m_map.emplace(m_categories[5], KoID(COMPOSITE_EQUIVALENCE          ,  PkString("Equivalence")));
    m_map.emplace(m_categories[5], KoID(COMPOSITE_ADDITIVE_SUBTRACTIVE ,  PkString("Additive Subtractive")));
    m_map.emplace(m_categories[5], KoID(COMPOSITE_EXCLUSION            ,  PkString("Exclusion")));
    m_map.emplace(m_categories[5], KoID(COMPOSITE_ARC_TANGENT          ,  PkString("Arcus Tangent")));
    m_map.emplace(m_categories[5], KoID(COMPOSITE_NEGATION             ,  PkString("Negation")));

    m_map.emplace(m_categories[6], koidCompositeOverStatic());
    m_map.emplace(m_categories[6], KoID(COMPOSITE_BEHIND          ,  PkString("Behind")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_GREATER         ,  PkString("Greater")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_OVERLAY         ,  PkString("Overlay")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_LAMBERT_LIGHTING, PkString("Lambert Lighting (Linear)")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_LAMBERT_LIGHTING_GAMMA_2_2, PkString("Lambert Lighting (Gamma 2.2)")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_ERASE           ,  PkString("Erase")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_ALPHA_DARKEN    ,  PkString("Alpha Darken")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_MARKER          ,  PkString("Marker")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_HARD_MIX        ,  PkString("Hard Mix")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_HARD_MIX_HDR    ,  PkString("Hard Mix HDR")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_HARD_MIX_PHOTOSHOP,  PkString("Hard Mix (Photoshop)")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_HARD_MIX_SOFTER_PHOTOSHOP,  PkString("Hard Mix Softer (Photoshop)")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_GRAIN_MERGE     ,  PkString("Grain Merge")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_GRAIN_EXTRACT   ,  PkString("Grain Extract")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_PARALLEL        ,  PkString("Parallel")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_ALLANON         ,  PkString("Allanon")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_GEOMETRIC_MEAN  ,  PkString("Geometric Mean")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_DESTINATION_ATOP,  PkString("Destination Atop")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_DESTINATION_IN  ,  PkString("Destination In")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_HARD_OVERLAY    ,  PkString("Hard Overlay")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_HARD_OVERLAY_HDR,  PkString("Hard Overlay HDR")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_INTERPOLATION   ,  PkString("Interpolation")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_INTERPOLATIONB  ,  PkString("Interpolation - 2X")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_PENUMBRAA       ,  PkString("Penumbra A")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_PENUMBRAB       ,  PkString("Penumbra B")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_PENUMBRAC       ,  PkString("Penumbra C")));
    m_map.emplace(m_categories[6], KoID(COMPOSITE_PENUMBRAD       ,  PkString("Penumbra D")));

    m_map.emplace(m_categories[7], KoID(COMPOSITE_BUMPMAP   ,  PkString("Bumpmap")));
    m_map.emplace(m_categories[7], KoID(COMPOSITE_COMBINE_NORMAL,  PkString("Combine Normal Map")));
    m_map.emplace(m_categories[7], KoID(COMPOSITE_DISSOLVE  ,  PkString("Dissolve")));
    m_map.emplace(m_categories[7], KoID(COMPOSITE_COPY_RED  ,  PkString("Copy Red")));
    m_map.emplace(m_categories[7], KoID(COMPOSITE_COPY_GREEN,  PkString("Copy Green")));
    m_map.emplace(m_categories[7], KoID(COMPOSITE_COPY_BLUE ,  PkString("Copy Blue")));
    m_map.emplace(m_categories[7], KoID(COMPOSITE_COPY      ,  PkString("Copy")));
    m_map.emplace(m_categories[7], KoID(COMPOSITE_TANGENT_NORMALMAP,  PkString("Tangent Normalmap")));

    m_map.emplace(m_categories[8], KoID(COMPOSITE_COLOR         ,  PkString("Color")));
    m_map.emplace(m_categories[8], KoID(COMPOSITE_HUE           ,  PkString("Hue")));
    m_map.emplace(m_categories[8], KoID(COMPOSITE_TINT          ,  PkString("Tint")));
    m_map.emplace(m_categories[8], KoID(COMPOSITE_SATURATION    ,  PkString("Saturation")));
    m_map.emplace(m_categories[8], KoID(COMPOSITE_LUMINIZE      ,  PkString("Luminosity")));
    m_map.emplace(m_categories[8], KoID(COMPOSITE_DEC_SATURATION,  PkString("Decrease Saturation")));
    m_map.emplace(m_categories[8], KoID(COMPOSITE_INC_SATURATION,  PkString("Increase Saturation")));
    m_map.emplace(m_categories[8], KoID(COMPOSITE_DEC_LUMINOSITY,  PkString("Decrease Luminosity")));
    m_map.emplace(m_categories[8], KoID(COMPOSITE_INC_LUMINOSITY,  PkString("Increase Luminosity")));

    m_map.emplace(m_categories[9], KoID(COMPOSITE_COLOR_HSI         ,  PkString("Color HSI")));
    m_map.emplace(m_categories[9], KoID(COMPOSITE_HUE_HSI           ,  PkString("Hue HSI")));
    m_map.emplace(m_categories[9], KoID(COMPOSITE_SATURATION_HSI    ,  PkString("Saturation HSI")));
    m_map.emplace(m_categories[9], KoID(COMPOSITE_INTENSITY         ,  PkString("Intensity")));
    m_map.emplace(m_categories[9], KoID(COMPOSITE_DEC_SATURATION_HSI,  PkString("Decrease Saturation HSI")));
    m_map.emplace(m_categories[9], KoID(COMPOSITE_INC_SATURATION_HSI,  PkString("Increase Saturation HSI")));
    m_map.emplace(m_categories[9], KoID(COMPOSITE_DEC_INTENSITY     ,  PkString("Decrease Intensity")));
    m_map.emplace(m_categories[9], KoID(COMPOSITE_INC_INTENSITY     ,  PkString("Increase Intensity")));

    m_map.emplace(m_categories[10], KoID(COMPOSITE_COLOR_HSL         ,  PkString("Color HSL")));
    m_map.emplace(m_categories[10], KoID(COMPOSITE_HUE_HSL           ,  PkString("Hue HSL")));
    m_map.emplace(m_categories[10], KoID(COMPOSITE_SATURATION_HSL    ,  PkString("Saturation HSL")));
    m_map.emplace(m_categories[10], KoID(COMPOSITE_LIGHTNESS         ,  PkString("Lightness")));
    m_map.emplace(m_categories[10], KoID(COMPOSITE_DEC_SATURATION_HSL,  PkString("Decrease Saturation HSL")));
    m_map.emplace(m_categories[10], KoID(COMPOSITE_INC_SATURATION_HSL,  PkString("Increase Saturation HSL")));
    m_map.emplace(m_categories[10], KoID(COMPOSITE_DEC_LIGHTNESS     ,  PkString("Decrease Lightness")));
    m_map.emplace(m_categories[10], KoID(COMPOSITE_INC_LIGHTNESS     ,  PkString("Increase Lightness")));

    m_map.emplace(m_categories[11], KoID(COMPOSITE_COLOR_HSV         ,  PkString("Color HSV")));
    m_map.emplace(m_categories[11], KoID(COMPOSITE_HUE_HSV           ,  PkString("Hue HSV")));
    m_map.emplace(m_categories[11], KoID(COMPOSITE_SATURATION_HSV    ,  PkString("Saturation HSV")));
    m_map.emplace(m_categories[11], KoID(COMPOSITE_VALUE             ,  PkString("Value")));
    m_map.emplace(m_categories[11], KoID(COMPOSITE_DEC_SATURATION_HSV,  PkString("Decrease Saturation HSV")));
    m_map.emplace(m_categories[11], KoID(COMPOSITE_INC_SATURATION_HSV,  PkString("Increase Saturation HSV")));
    m_map.emplace(m_categories[11], KoID(COMPOSITE_DEC_VALUE         ,  PkString("Decrease Value")));
    m_map.emplace(m_categories[11], KoID(COMPOSITE_INC_VALUE         ,  PkString("Increase Value")));
    
    m_map.emplace(m_categories[12], KoID(COMPOSITE_REFLECT          ,  PkString("Reflect")));
    m_map.emplace(m_categories[12], KoID(COMPOSITE_GLOW             ,  PkString("Glow")));
    m_map.emplace(m_categories[12], KoID(COMPOSITE_FREEZE           ,  PkString("Freeze")));
    m_map.emplace(m_categories[12], KoID(COMPOSITE_HEAT             ,  PkString("Heat")));
    m_map.emplace(m_categories[12], KoID(COMPOSITE_GLEAT            ,  PkString("Glow-Heat")));
    m_map.emplace(m_categories[12], KoID(COMPOSITE_HELOW            ,  PkString("Heat-Glow")));
    m_map.emplace(m_categories[12], KoID(COMPOSITE_REEZE            ,  PkString("Reflect-Freeze")));
    m_map.emplace(m_categories[12], KoID(COMPOSITE_FRECT            ,  PkString("Freeze-Reflect")));
    m_map.emplace(m_categories[12], KoID(COMPOSITE_FHYRD            ,  PkString("Heat-Glow & Freeze-Reflect Hybrid")));
}

const KoCompositeOpRegistry& KoCompositeOpRegistry::instance()
{
    static KoCompositeOpRegistry registry;
    return registry;
}

KoID KoCompositeOpRegistry::getDefaultCompositeOp() const
{
    return koidCompositeOverStatic();
}

KoID KoCompositeOpRegistry::getKoID(const PkString& compositeOpID) const
{
    KoIDMap::const_iterator itr = findCompositeOp(m_map, KoID(compositeOpID));
    return (itr != m_map.end()) ? itr->second : KoID();
}

PkString KoCompositeOpRegistry::getCompositeOpDisplayName(const PkString& compositeOpID) const
{
    // In and Out are created in lcms2engine but not registered in KoCompositeOpRegistry.
    // FIXME: 这两个名字目前没有注册进 KoCompositeOpRegistry，暂用英文原名。
    if (compositeOpID == COMPOSITE_IN) {
        return PkString("In");
    } else if (compositeOpID == COMPOSITE_OUT) {
        return PkString("Out");
    }

    const PkString name = getKoID(compositeOpID).name();
    if (name.isEmpty()) {
        warnPigment << "Got null display name for composite op" << compositeOpID;
        return compositeOpID;
    }
    return name;
}

KoCompositeOpRegistry::KoIDMap KoCompositeOpRegistry::getCompositeOps() const
{
    return m_map;
}

KoCompositeOpRegistry::KoIDMap KoCompositeOpRegistry::getLayerStylesCompositeOps() const
{
    PkVector<PkString> ids;

    // not available via the blending modes list in Krita
    // ids << COMPOSITE_PASS_THROUGH;

    ids << COMPOSITE_OVER;
    ids << COMPOSITE_DISSOLVE;
    ids << COMPOSITE_DARKEN;
    ids << COMPOSITE_MULT;
    ids << COMPOSITE_BURN;
    ids << COMPOSITE_LINEAR_BURN;
    ids << COMPOSITE_DARKER_COLOR;
    ids << COMPOSITE_LIGHTEN;
    ids << COMPOSITE_SCREEN;
    ids << COMPOSITE_DODGE;
    ids << COMPOSITE_LINEAR_DODGE;
    ids << COMPOSITE_LIGHTER_COLOR;
    ids << COMPOSITE_OVERLAY;
    ids << COMPOSITE_SOFT_LIGHT_PHOTOSHOP;
    ids << COMPOSITE_HARD_LIGHT;
    ids << COMPOSITE_VIVID_LIGHT;
    ids << COMPOSITE_LINEAR_LIGHT;
    ids << COMPOSITE_PIN_LIGHT;
    ids << COMPOSITE_HARD_MIX_PHOTOSHOP;
    ids << COMPOSITE_DIFF;
    ids << COMPOSITE_EXCLUSION;
    ids << COMPOSITE_SUBTRACT;
    ids << COMPOSITE_DIVIDE;
    ids << COMPOSITE_HUE;
    ids << COMPOSITE_SATURATION;
    ids << COMPOSITE_COLOR;
    ids << COMPOSITE_LUMINIZE;

    KoIDMap result;
    for (const PkString &id : ids) {
        KoIDMap::const_iterator iter = findCompositeOp(m_map, KoID(id));
        KIS_SAFE_ASSERT_RECOVER(iter != m_map.end()) { continue; }

        result.emplace(iter->first, iter->second);
    }

    return result;
}

KoCompositeOpRegistry::KoIDList KoCompositeOpRegistry::getCategories() const
{
    return m_categories;
}

PkString  KoCompositeOpRegistry::getCategoryDisplayName(const PkString& categoryID) const
{
    KoIDList::const_iterator itr = std::find(m_categories.begin(), m_categories.end(), KoID(categoryID));
    const PkString name = (itr != m_categories.end()) ? itr->name() : PkString();
    if (name.isEmpty()) {
        warnPigment << "Got null display name for composite op category" << categoryID;
        return categoryID;
    }
    return name;
}

KoCompositeOpRegistry::KoIDList KoCompositeOpRegistry::getCompositeOps(const KoID& category, const KoColorSpace* colorSpace) const
{
    const qint32              num = static_cast<qint32>(m_map.count(category));
    KoIDMap::const_iterator beg = m_map.lower_bound(category);
    KoIDMap::const_iterator end = m_map.upper_bound(category);

    KoIDList list;
    list.reserve(num);

    if(colorSpace) {
        for(; beg!=end; ++beg){
            if(colorSpace->hasCompositeOp(beg->second.id()))
                list.push_back(beg->second);
        }
    }
    else {
        for(; beg!=end; ++beg)
            list.push_back(beg->second);
    }

    return list;
}

KoCompositeOpRegistry::KoIDList KoCompositeOpRegistry::getCompositeOps(const KoColorSpace* colorSpace) const
{
    KoIDMap::const_iterator beg = m_map.begin();
    KoIDMap::const_iterator end = m_map.end();

    KoIDList list;
    list.reserve(m_map.size());

    if(colorSpace) {
        for(; beg!=end; ++beg){
            if(colorSpace->hasCompositeOp(beg->second.id()))
                list.push_back(beg->second);
        }
    }
    else {
        for(; beg!=end; ++beg)
            list.push_back(beg->second);
    }

    return list;
}

bool KoCompositeOpRegistry::colorSpaceHasCompositeOp(const KoColorSpace* colorSpace, const KoID& compositeOp) const
{
    return colorSpace ? colorSpace->hasCompositeOp(compositeOp.id()) : false;
}
