/*
 * SPDX-FileCopyrightText: 2005 Adrian Page <adrian@pagenet.plus.com>
 * SPDX-FileCopyrightText: 2011 Silvio Heinrich <plassy@web.de>
 *
 * SPDX-License-Identifier: LGPL-2.1-or-later
*/
#ifndef KOCOMPOSITEOPREGISTRY_H
#define KOCOMPOSITEOPREGISTRY_H

#include <PkString.h>
#include <PkList.h>
#include <map>

#include "kritapigment_export.h"

class KoColorSpace;
#include <KoID.h>

// TODO : convert this data blob into a modern design with an enum class.
// This will reduce the need for runtime string comparisons.

const PkString COMPOSITE_OVER         = "normal";
const PkString COMPOSITE_ERASE        = "erase";
const PkString COMPOSITE_IN           = "in";
const PkString COMPOSITE_OUT          = "out";
const PkString COMPOSITE_ALPHA_DARKEN = "alphadarken";
const PkString COMPOSITE_DESTINATION_IN = "destination-in";
const PkString COMPOSITE_DESTINATION_ATOP = "destination-atop";

const PkString COMPOSITE_XOR                   = "xor";
const PkString COMPOSITE_OR                    = "or";
const PkString COMPOSITE_AND                   = "and";
const PkString COMPOSITE_NAND                  = "nand";
const PkString COMPOSITE_NOR                   = "nor";
const PkString COMPOSITE_XNOR                  = "xnor";
const PkString COMPOSITE_IMPLICATION           = "implication";
const PkString COMPOSITE_NOT_IMPLICATION       = "not_implication";
const PkString COMPOSITE_CONVERSE              = "converse";
const PkString COMPOSITE_NOT_CONVERSE          = "not_converse";

const PkString COMPOSITE_PLUS                  = "plus";
const PkString COMPOSITE_MINUS                 = "minus";
const PkString COMPOSITE_ADD                   = "add";
const PkString COMPOSITE_SUBTRACT              = "subtract";
const PkString COMPOSITE_INVERSE_SUBTRACT      = "inverse_subtract";
const PkString COMPOSITE_DIFF                  = "diff";
const PkString COMPOSITE_MULT                  = "multiply";
const PkString COMPOSITE_DIVIDE                = "divide";
const PkString COMPOSITE_ARC_TANGENT           = "arc_tangent";
const PkString COMPOSITE_GEOMETRIC_MEAN        = "geometric_mean";
const PkString COMPOSITE_ADDITIVE_SUBTRACTIVE  = "additive_subtractive";
const PkString COMPOSITE_NEGATION              = "negation";

const PkString COMPOSITE_MOD                = "modulo";
const PkString COMPOSITE_MOD_CON            = "modulo_continuous";
const PkString COMPOSITE_DIVISIVE_MOD       = "divisive_modulo";
const PkString COMPOSITE_DIVISIVE_MOD_CON   = "divisive_modulo_continuous";
const PkString COMPOSITE_MODULO_SHIFT       = "modulo_shift";
const PkString COMPOSITE_MODULO_SHIFT_CON   = "modulo_shift_continuous";

const PkString COMPOSITE_EQUIVALENCE   = "equivalence";
const PkString COMPOSITE_ALLANON       = "allanon";
const PkString COMPOSITE_PARALLEL      = "parallel";
const PkString COMPOSITE_GRAIN_MERGE   = "grain_merge";
const PkString COMPOSITE_GRAIN_EXTRACT = "grain_extract";
const PkString COMPOSITE_EXCLUSION     = "exclusion";
const PkString COMPOSITE_HARD_MIX      = "hard mix";
const PkString COMPOSITE_HARD_MIX_HDR  = "hard_mix_hdr";
const PkString COMPOSITE_HARD_MIX_PHOTOSHOP = "hard_mix_photoshop";
const PkString COMPOSITE_HARD_MIX_SOFTER_PHOTOSHOP = "hard_mix_softer_photoshop";
const PkString COMPOSITE_OVERLAY       = "overlay";
const PkString COMPOSITE_BEHIND        = "behind";
const PkString COMPOSITE_GREATER       = "greater";
const PkString COMPOSITE_HARD_OVERLAY  = "hard overlay";
const PkString COMPOSITE_HARD_OVERLAY_HDR  = "hard_overlay_hdr";
const PkString COMPOSITE_INTERPOLATION = "interpolation";
const PkString COMPOSITE_INTERPOLATIONB = "interpolation 2x";
const PkString COMPOSITE_PENUMBRAA     = "penumbra a";
const PkString COMPOSITE_PENUMBRAB     = "penumbra b";
const PkString COMPOSITE_PENUMBRAC     = "penumbra c";
const PkString COMPOSITE_PENUMBRAD     = "penumbra d";
const PkString COMPOSITE_MARKER        = "marker";

const PkString COMPOSITE_DARKEN      = "darken";
const PkString COMPOSITE_BURN        = "burn";//this is also known as 'color burn'.
const PkString COMPOSITE_LINEAR_BURN = "linear_burn";
const PkString COMPOSITE_GAMMA_DARK  = "gamma_dark";
const PkString COMPOSITE_SHADE_IFS_ILLUSIONS = "shade_ifs_illusions";
const PkString COMPOSITE_FOG_DARKEN_IFS_ILLUSIONS = "fog_darken_ifs_illusions";
const PkString COMPOSITE_EASY_BURN        = "easy burn";

const PkString COMPOSITE_LIGHTEN      = "lighten";
const PkString COMPOSITE_DODGE        = "dodge";
const PkString COMPOSITE_DODGE_HDR    = "dodge_hdr";
const PkString COMPOSITE_LINEAR_DODGE = "linear_dodge";
const PkString COMPOSITE_SCREEN       = "screen";
const PkString COMPOSITE_HARD_LIGHT   = "hard_light";
const PkString COMPOSITE_SOFT_LIGHT_IFS_ILLUSIONS = "soft_light_ifs_illusions";
const PkString COMPOSITE_SOFT_LIGHT_PEGTOP_DELPHI = "soft_light_pegtop_delphi";
const PkString COMPOSITE_SOFT_LIGHT_PHOTOSHOP = "soft_light";
const PkString COMPOSITE_SOFT_LIGHT_SVG  = "soft_light_svg";
const PkString COMPOSITE_GAMMA_LIGHT  = "gamma_light";
const PkString COMPOSITE_GAMMA_ILLUMINATION  = "gamma_illumination";
const PkString COMPOSITE_VIVID_LIGHT  = "vivid_light";
const PkString COMPOSITE_VIVID_LIGHT_HDR  = "vivid_light_hdr";
const PkString COMPOSITE_FLAT_LIGHT   = "flat_light";
const PkString COMPOSITE_LINEAR_LIGHT = "linear light";
const PkString COMPOSITE_PIN_LIGHT    = "pin_light";
const PkString COMPOSITE_PNORM_A        = "pnorm_a";
const PkString COMPOSITE_PNORM_B        = "pnorm_b";
const PkString COMPOSITE_SUPER_LIGHT  = "super_light";
const PkString COMPOSITE_TINT_IFS_ILLUSIONS = "tint_ifs_illusions";
const PkString COMPOSITE_FOG_LIGHTEN_IFS_ILLUSIONS = "fog_lighten_ifs_illusions";
const PkString COMPOSITE_EASY_DODGE        = "easy dodge";
const PkString COMPOSITE_LUMINOSITY_SAI        = "luminosity_sai";


const PkString COMPOSITE_HUE            = "hue";
const PkString COMPOSITE_COLOR          = "color";
const PkString COMPOSITE_TINT           = "tint";
const PkString COMPOSITE_SATURATION     = "saturation";
const PkString COMPOSITE_INC_SATURATION = "inc_saturation";
const PkString COMPOSITE_DEC_SATURATION = "dec_saturation";
const PkString COMPOSITE_LUMINIZE       = "luminize";
const PkString COMPOSITE_INC_LUMINOSITY = "inc_luminosity";
const PkString COMPOSITE_DEC_LUMINOSITY = "dec_luminosity";

const PkString COMPOSITE_HUE_HSV            = "hue_hsv";
const PkString COMPOSITE_COLOR_HSV          = "color_hsv";
const PkString COMPOSITE_SATURATION_HSV     = "saturation_hsv";
const PkString COMPOSITE_INC_SATURATION_HSV = "inc_saturation_hsv";
const PkString COMPOSITE_DEC_SATURATION_HSV = "dec_saturation_hsv";
const PkString COMPOSITE_VALUE              = "value";
const PkString COMPOSITE_INC_VALUE          = "inc_value";
const PkString COMPOSITE_DEC_VALUE          = "dec_value";

const PkString COMPOSITE_HUE_HSL            = "hue_hsl";
const PkString COMPOSITE_COLOR_HSL          = "color_hsl";
const PkString COMPOSITE_SATURATION_HSL     = "saturation_hsl";
const PkString COMPOSITE_INC_SATURATION_HSL = "inc_saturation_hsl";
const PkString COMPOSITE_DEC_SATURATION_HSL = "dec_saturation_hsl";
const PkString COMPOSITE_LIGHTNESS          = "lightness";
const PkString COMPOSITE_INC_LIGHTNESS      = "inc_lightness";
const PkString COMPOSITE_DEC_LIGHTNESS      = "dec_lightness";

const PkString COMPOSITE_HUE_HSI            = "hue_hsi";
const PkString COMPOSITE_COLOR_HSI          = "color_hsi";
const PkString COMPOSITE_SATURATION_HSI     = "saturation_hsi";
const PkString COMPOSITE_INC_SATURATION_HSI = "inc_saturation_hsi";
const PkString COMPOSITE_DEC_SATURATION_HSI = "dec_saturation_hsi";
const PkString COMPOSITE_INTENSITY          = "intensity";
const PkString COMPOSITE_INC_INTENSITY      = "inc_intensity";
const PkString COMPOSITE_DEC_INTENSITY      = "dec_intensity";

const PkString COMPOSITE_COPY         = "copy";
const PkString COMPOSITE_COPY_RED     = "copy_red";
const PkString COMPOSITE_COPY_GREEN   = "copy_green";
const PkString COMPOSITE_COPY_BLUE    = "copy_blue";
const PkString COMPOSITE_TANGENT_NORMALMAP    = "tangent_normalmap";

const PkString COMPOSITE_COLORIZE     = "colorize";
const PkString COMPOSITE_BUMPMAP      = "bumpmap";
const PkString COMPOSITE_COMBINE_NORMAL = "combine_normal";
const PkString COMPOSITE_CLEAR        = "clear";
const PkString COMPOSITE_DISSOLVE     = "dissolve";
const PkString COMPOSITE_DISPLACE     = "displace";
const PkString COMPOSITE_NO           = "nocomposition";
const PkString COMPOSITE_PASS_THROUGH = "pass through"; // XXX: not implemented anywhere yet
const PkString COMPOSITE_DARKER_COLOR = "darker color";
const PkString COMPOSITE_LIGHTER_COLOR = "lighter color";
const PkString COMPOSITE_UNDEF        = "undefined";

const PkString COMPOSITE_REFLECT   = "reflect";
const PkString COMPOSITE_GLOW      = "glow";
const PkString COMPOSITE_FREEZE    = "freeze";
const PkString COMPOSITE_HEAT      = "heat";
const PkString COMPOSITE_GLEAT     = "glow_heat";
const PkString COMPOSITE_HELOW     = "heat_glow";
const PkString COMPOSITE_REEZE     = "reflect_freeze";
const PkString COMPOSITE_FRECT     = "freeze_reflect";
const PkString COMPOSITE_FHYRD     = "heat_glow_freeze_reflect_hybrid";

const PkString COMPOSITE_LAMBERT_LIGHTING   = "lambert_lighting";
const PkString COMPOSITE_LAMBERT_LIGHTING_GAMMA_2_2   = "lambert_lighting_gamma2.2";


class KRITAPIGMENT_EXPORT KoCompositeOpRegistry
{
    typedef std::multimap<KoID,KoID> KoIDMap;
    typedef PkList<KoID>          KoIDList;

public:
    KoCompositeOpRegistry();
    static const KoCompositeOpRegistry& instance();

    KoID     getDefaultCompositeOp() const;
    KoID     getKoID(const PkString& compositeOpID) const;
    PkString  getCompositeOpDisplayName(const PkString& compositeOpID) const;
    KoIDMap  getCompositeOps() const;
    KoIDMap  getLayerStylesCompositeOps() const;
    KoIDList getCategories() const;
    PkString  getCategoryDisplayName(const PkString& categoryID) const;
    KoIDList getCompositeOps(const KoColorSpace* colorSpace) const;
    KoIDList getCompositeOps(const KoID& category, const KoColorSpace* colorSpace=0) const;
    bool     colorSpaceHasCompositeOp(const KoColorSpace* colorSpace, const KoID& compositeOp) const;

    template<class TKoIdIterator>
    KoIDList filterCompositeOps(TKoIdIterator begin, TKoIdIterator end, const KoColorSpace* colorSpace, bool removeInvalidOps=true) const {
        KoIDList list;

        for(; begin!=end; ++begin){
            if (colorSpaceHasCompositeOp(colorSpace, *begin) == removeInvalidOps) {
                list.push_back(*begin);
            }
        }

        return list;
    }

private:
    KoIDList m_categories;
    KoIDMap  m_map;
};


#endif // KOCOMPOSITEOPREGISTRY_H
