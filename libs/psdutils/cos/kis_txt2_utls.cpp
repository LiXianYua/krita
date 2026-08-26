/*
 *  SPDX-FileCopyrightText: 2023 Wolthera van Hövell tot Westerflier <griffinvalley@gmail.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include "kis_txt2_utls.h"
#include <PkVariant.h>
#include <PkStringList.h>
#include <PkRect.h>

#include <algorithm>
#include <map>

// 哈希 value(key) 的 Pk 版：缺 key 返回 Invalid null PkVariant。
static PkVariant pkHashValue(const PkVariantHash &h, const PkString &key) {
    const auto it = h.find(key);
    return it != h.end() ? it->second : PkVariant();
}
// 哈希 value(key, defaultValue) 的 Pk 版。
static PkVariant pkHashValueDefault(const PkVariantHash &h, const PkString &key, const PkVariant &def) {
    const auto it = h.find(key);
    return it != h.end() ? it->second : def;
}
// 哈希 contains(key) 的 Pk 版。
static bool pkHashContains(const PkVariantHash &h, const PkString &key) {
    return h.find(key) != h.end();
}
// 旧 map 的 keys().contains(key) 的 Pk 版（std::map）。
static bool pkMapContains(const std::map<PkString, PkString> &m, const PkString &key) {
    return m.find(key) != m.end();
}
// 旧 map 的 value(key) 的 Pk 版：缺 key 返回空 PkString（对齐旧 map 的 value 默认值）。
static PkString pkMapValue(const std::map<PkString, PkString> &m, const PkString &key) {
    const auto it = m.find(key);
    return it != m.end() ? it->second : PkString();
}
// 列表 value(index) 的 Pk 版：越界返回 Invalid null PkVariant。
static PkVariant pkListValue(const PkVariantList &l, size_t index) {
    return index < l.size() ? l[index] : PkVariant();
}

PkVariantHash uncompressColor(const PkVariantHash object) {
    PkVariantHash newObject;
    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;
        if (key == "/0") {
            const PkVariantHash color = val.toHash();
            PkVariantHash newColor;
            for (const auto &cit : color) {
                const PkString ckey = cit.first;
                const PkVariant cval = cit.second;
                if (ckey == "/1") {
                    newColor[PkString("/Values")] = cval;
                } else if (ckey == "/0") {
                    newColor[PkString("/Type")] = cval;
                } else {
                    newColor[ckey] = cval;
                }
            }
            newObject[PkString("/Color")] = newColor;
        } else if (key == "/99") {
            newObject[PkString("/StreamTag")] = val;
        } else {
            newObject[key] = val;
        }
    }
    return newObject;
}

PkVariantHash uncompressStyleSheetFeatures(const PkVariantHash object) {
    PkVariantHash newObject;

    const std::map<PkString, PkString> keyList{
        {"/0", "/Font"},
        {"/1", "/FontSize"},
        {"/2", "/FauxBold"},
        {"/3", "/FauxItalic"},
        {"/4", "/AutoLeading"},

        {"/5", "/Leading"},
        {"/6", "/HorizontalScale"},
        {"/7", "/VerticalScale"},
        {"/8", "/Tracking"},
        {"/9", "/BaselineShift"},

        {"/10", "/CharacterRotation"},
        {"/11", "/AutoKern"},
        {"/12", "/FontCaps"},
        {"/13", "/FontBaseline"},
        {"/14", "/FontOTPosition"},

        {"/15", "/StrikethroughPosition"},
        {"/16", "/UnderlinePosition"},
        {"/17", "/UnderlineOffset"},
        {"/18", "/Ligatures"},
        {"/19", "/DiscretionaryLigatures"},

        {"/20", "/ContextualLigatures"},
        {"/21", "/AlternateLigatures"},
        {"/22", "/OldStyle"},
        {"/23", "/Fractions"},
        {"/24", "/Ordinals"},

        {"/25", "/Swash"},
        {"/26", "/Titling"},
        {"/27", "/ConnectionForms"},
        {"/28", "/StylisticAlternates"},
        {"/29", "/Ornaments"},

        {"/30", "/FigureStyle"},
        {"/31", "/ProportionalMetrics"},
        {"/32", "/Kana"},
        {"/33", "/Italics"},
        {"/34", "/Ruby"},

        {"/35", "/BaselineDirection"},
        {"/36", "/Tsume"},
        {"/37", "/StyleRunAlignment"},
        {"/38", "/Language"},
        {"/39", "/JapaneseAlternateFeature"},

        {"/40", "/EnableWariChu"},
        {"/41", "/WariChuLineCount"},
        {"/42", "/WariChuLineGap"},
        {"/43", "/WariChuSubLineAmount"},
        {"/44", "/WariChuWidowAmount"},

        {"/45", "/WariChuOrphanAmount"},
        {"/46", "/WariChuJustification"},
        {"/47", "/TCYUpDownAdjustment"},
        {"/48", "/TCYLeftRightAdjustment"},
        {"/49", "/LeftAki"},

        {"/50", "/RightAki"},
        {"/51", "/JiDori"},
        {"/52", "/NoBreak"},
        //{"/53", "/FillColor"},
        //{"/54", "/StrokeColor"},

        {"/55", "/Blend"},
        {"/56", "/FillFlag"},
        {"/57", "/StrokeFlag"},
        {"/58", "/FillFirst"},
        {"/59", "/FillOverPrint"},

        {"/60", "/StrokeOverPrint"},
        {"/61", "/LineCap"},
        {"/62", "/LineJoin"},
        {"/63", "/LineWidth"},
        {"/64", "/MiterLimit"},

        {"/65", "/LineDashOffset"},
        {"/66", "/LineDashArray"},
        {"/67", "/Type"},
        {"/68", "/Kashidas"},
        {"/69", "/DirOverride"},

        {"/70", "/DigitSet"},
        {"/71", "/DiacVPos"},
        {"/72", "/DiacXOffset"},
        {"/73", "/DiacYOffset"},
        {"/74", "/OverlapSwash"},

        {"/75", "/JustificationAlternates"},
        {"/76", "/StretchedAlternates"},
        {"/77", "/FillVisibleFlag"},
        {"/78", "/StrokeVisibleFlag"},
        //{"/79", "/FillBackgroundColor"},

        {"/80", "/FillBackgroundFlag"},
        {"/81", "/UnderlineStyle"},
        {"/82", "/DashedUnderlineGapLength"},
        {"/83", "/DashedUnderlineDashLength"},
        {"/84", "/SlashedZero"},

        {"/85", "/StylisticSets"},
        {"/86", "/CustomFeature"},
        {"/87", "/MarkYDistFromBaseline"},
        {"/88", "/AutoMydfb"},
        {"/89", "/RefFontSize"},

        {"/90", "/FontSizeRefType"},
        // 原 map 里 /92 出现两次（"/MagicLineGap" 先、"/MagicWordGap" 后），map 重复 key
        // 保留最后一个；std::map 保留第一个，所以这里只写胜出的 "/MagicWordGap"。
        {"/92", "/MagicWordGap"},
    };

    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;
        if (key == "/53") {
            newObject[PkString("/FillColor")] = uncompressColor(val.toHash());
        } else if (key == "/54") {
            newObject[PkString("/StrokeColor")] = uncompressColor(val.toHash());
        } else if (key == "/79") {
            newObject[PkString("/FillBackgroundColor")] = uncompressColor(val.toHash());
        } else if (pkMapContains(keyList, key)) {
            newObject[pkMapValue(keyList, key)] = val;
        } else {
            newObject[key] = val;
        }
    }
    return newObject;
}

PkVariantHash uncompressParagraphSheetFeatures(const PkVariantHash object) {
    PkVariantHash newObject;

    const std::map<PkString, PkString> keyList{
        {"/0", "/Justification"},
        {"/1", "/FirstLineIndent"},
        {"/2", "/StartIndent"},
        {"/3", "/EndIndent"},
        {"/4", "/SpaceBefore"},

        {"/5", "/SpaceAfter"},
        {"/6", "/DropCaps"},
        {"/7", "/AutoLeading"},
        {"/8", "/LeadingType"},
        {"/9", "/AutoHyphenate"},

        {"/10", "/HyphenatedWordSize"},
        {"/11", "/PreHyphen"},
        {"/12", "/PostHyphen"},
        {"/13", "/ConsecutiveHyphens"},
        {"/14", "/Zone"},

        {"/15", "/HyphenateCapitalized"},
        {"/16", "/HyphenationPreference"},
        {"/17", "/WordSpacing"},
        {"/18", "/LetterSpacing"},
        {"/19", "/GlyphSpacing"},

        {"/20", "/SingleWordJustification"},
        {"/21", "/Hanging"},
        {"/22", "/AutoTCY"},
        {"/23", "/KeepTogether"},
        {"/24", "/BurasagariType"},

        {"/25", "/KinsokuOrder"},
        {"/26", "/KurikaeshiMojiShori"},
        {"/27", "/Kinsoku"},
        {"/28", "/MojiKumiTable"},
        {"/29", "/EveryLineComposer"},

        {"/30", "/TabStops"},
        {"/31", "/DefaultTabWidth"},
        //{"/32", "/DefaultStyle"},
        {"/33", "/ParagraphDirection"},
        {"/34", "/JustificationMethod"},

        {"/35", "/ComposerEngine"},
        {"/36", "/ListStyle"},
        {"/37", "/ListTier"},
        {"/38", "/ListSkip"},
        {"/39", "/ListOffset"},

        {"/40", "/KashidaWidth"}
    };

    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;
        if (key == "/32") {
            newObject[PkString("/DefaultStyle")] = uncompressStyleSheetFeatures(val.toHash());
        } else if (pkMapContains(keyList, key)) {
            newObject[pkMapValue(keyList, key)] = val;
        } else {
            newObject[key] = val;
        }
    }
    return newObject;
}

PkVariantHash uncompressKeysStyleSheetSet(const PkVariantHash object) {
    PkVariantHash newObject;

    const PkVariantList resources = pkHashValue(object, PkString("/0")).toList();
    PkVariantList newResources;
    const std::map<PkString, PkString> keyList {{"/0", "/Name"}, {"/5", "/Parent"}, {"/97", "/UUID"}};

    for (const PkVariant &val : resources) {
        const PkVariantHash resource = pkHashValue(val.toHash(), PkString("/0")).toHash();
        PkVariantHash newResource;
        for (const auto &rit : resource) {
            const PkString key = rit.first;
            const PkVariant rdVal = rit.second;
            if (key == "/6") {
                newResource[PkString("/Features")] = uncompressStyleSheetFeatures(rdVal.toHash());
            } else if (pkMapContains(keyList, key)) {
                newResource[pkMapValue(keyList, key)] = rdVal;
            } else {
                newResource[key] = rdVal;
            }
        }

        newResources.push_back(PkVariantHash({{PkString("/Resource"), newResource}}));
    }
    newObject[PkString("/Resources")] = newResources;
    return newObject;
}

PkVariantHash uncompressKeysParagraphSheetSet(const PkVariantHash object) {
    PkVariantHash newObject;
    const PkVariantList resources = pkHashValue(object, PkString("/0")).toList();
    PkVariantList newResources;

    const std::map<PkString, PkString> keyList {{"/0", "/Name"}, {"/6", "/Parent"}, {"/97", "/UUID"}};

    for (const PkVariant &val : resources) {
        const PkVariantHash resource = pkHashValue(val.toHash(), PkString("/0")).toHash();
        PkVariantHash newResource;
        for (const auto &rit : resource) {
            const PkString key = rit.first;
            const PkVariant rdVal = rit.second;
            if (key == "/5") {
                newResource[PkString("/Features")] = uncompressParagraphSheetFeatures(rdVal.toHash());
            } else if (pkMapContains(keyList, key)) {
                newResource[pkMapValue(keyList, key)] = rdVal;
            } else {
                newResource[key] = rdVal;
            }
        }

        newResources.push_back(PkVariantHash({{PkString("/Resource"), newResource}}));
    }
    newObject[PkString("/Resources")] = newResources;
    return newObject;
}

PkVariantHash uncompressTextFrameData(const PkVariantHash object) {
    PkVariantHash newObject;
    const std::map<PkString, PkString> keyList {
        {"/0", "/Type"},
        {"/1", "/LineOrientation"},
        {"/2", "/FrameMatrix"},
        {"/3", "/RowCount"},
        {"/4", "/ColumnCount"},

        {"/5", "/RowMajorOrder"},
        {"/6", "/TextOnPathTRange"},
        {"/7", "/RowGutter"},
        {"/8", "/ColumnGutter"},
        {"/9", "/Spacing"},

        {"/10", "/FirstBaseAlignment"},
       // {"/11", "/PathData"},
        //{"/12", ""},
        {"/13", "/_VerticalAlignment"},
    };

    const std::map<PkString, PkString> pathDataList {
        {"/0", "/Flip"},
        {"/1", "/Effect"},
        {"/2", "/Alignment"},
        {"/4", "/_Spacing"},
        {"/18", "/_Spacing2"},
    };

    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;
        if (key == "/11") {
            const PkVariantHash data = val.toHash();
            PkVariantHash newData;
            for (const auto &dit : data) {
                const PkString key2 = dit.first;
                if (pkMapContains(pathDataList, key2)) {
                    newData[pkMapValue(pathDataList, key2)] = dit.second;
                } else {
                    newData[key2] = dit.second;
                }
            }
            newObject[PkString("/PathData")] = newData;
        } else if (pkMapContains(keyList, key)) {
            newObject[pkMapValue(keyList, key)] = val;
        } else {
            newObject[key] = val;
        }
    }
    return newObject;
}

PkVariantHash uncompressKeysTextFrameSet(const PkVariantHash object) {
    PkVariantHash newObject;

    const PkVariantList resources = pkHashValue(object, PkString("/0")).toList();
    PkVariantList newResources;
    const std::map<PkString, PkString> keyList {
        {"/0", "/_Position"},
        {"/1", "/Bezier"},
        //{"/2", "/Data"},
        {"/97", "/UUID"}
    };

    for (const PkVariant &val : resources) {
        const PkVariantHash resource = pkHashValue(val.toHash(), PkString("/0")).toHash();
        PkVariantHash newResource;
        for (const auto &rit : resource) {
            const PkString key2 = rit.first;
            const PkVariant rdVal = rit.second;
            if (key2 == "/5") {
                newResource[PkString("/Features")] = uncompressStyleSheetFeatures(rdVal.toHash());
            } else if (key2 == "/1") {
                const PkVariant pList = pkHashValue(rdVal.toHash(), PkString("/0"));

                newResource[PkString("/Bezier")] = PkVariantHash({{PkString("/Points"), pList}});
            } else if (key2 == "/2") {
                newResource[PkString("/Data")] = uncompressTextFrameData(rdVal.toHash());
            } else if (pkMapContains(keyList, key2)) {
                newResource[pkMapValue(keyList, key2)] = rdVal;
            } else {
                newResource[key2] = rdVal;
            }
        }

        newResources.push_back(PkVariantHash({{PkString("/Resource"), newResource}}));
    }
    newObject[PkString("/Resources")] = newResources;
    return newObject;
}

PkVariantHash uncompressKeysKinsokuSet(const PkVariantHash object) {
    PkVariantHash newObject;

    const PkVariantList resources = pkHashValue(object, PkString("/0")).toList();
    PkVariantList newResources;

    const std::map<PkString, PkString> keyList {{"/0", "/Name"}, {"/5", "/Data"}};
    const std::map<PkString, PkString> idKeyList {{"/0", "/NoStart"}, {"/1", "/NoEnd"}, {"/2", "/Keep"}, {"/3", "/Hanging"}, {"/4", "/PredefinedTag"}};


    for (const PkVariant &val : resources) {
        const PkVariantHash resource = pkHashValue(val.toHash(), PkString("/0")).toHash();
        PkVariantHash newResource;
        for (const auto &rit : resource) {
            const PkString key = rit.first;
            const PkVariant rdVal = rit.second;
            if (key == "/5") {
                const PkVariantHash id = rdVal.toHash();
                PkVariantHash newId;
                for (const auto &iit : id) {
                    const PkString key2 = iit.first;
                    const PkVariant idVal = iit.second;
                    if (pkMapContains(idKeyList, key2)) {
                        newId[pkMapValue(idKeyList, key2)] = idVal;
                    } else {
                        newId[key2] = idVal;
                    }
                }
                newResource[PkString("/Data")] = newId;
            } else if (pkMapContains(keyList, key)) {
                newResource[pkMapValue(keyList, key)] = rdVal;
            } else {
                newResource[key] = rdVal;
            }
        }

        newResources.push_back(PkVariantHash({{PkString("/Resource"), newResource}}));
    }
    newObject[PkString("/Resources")] = newResources;
    return newObject;
}

PkVariantHash uncompressKeysMojiKumiTableSet(const PkVariantHash object) {
    PkVariantHash newObject;

    for (const auto &it : object) {
        newObject[it.first] = it.second;
    }
    return newObject;
}

PkVariantHash uncompressKeysMojiKumiCodeToClassSet(const PkVariantHash object) {
    PkVariantHash newObject;

    for (const auto &it : object) {
        newObject[it.first] = it.second;
    }
    return newObject;
}

PkVariantHash uncompressKeysFontSet(const PkVariantHash object) {
    PkVariantHash newObject;

    const PkVariantList resources = pkHashValue(object, PkString("/0")).toList();
    PkVariantList newResources;

    const std::map<PkString, PkString> keyList {{"/99", "/StreamTag"}, {"/97", "/UUID"}};
    const std::map<PkString, PkString> idKeyList {{"/0", "/Name"}, {"/2", "/Type"}, {"/4", "/MMAxis"}, {"/5", "/VersionString"}};


    for (const PkVariant &val : resources) {
        const PkVariantHash resource = pkHashValue(val.toHash(), PkString("/0")).toHash();
        PkVariantHash newResource;
        for (const auto &rit : resource) {
            const PkString key = rit.first;
            const PkVariant rdVal = rit.second;
            if (key == "/0") {
                const PkVariantHash id = rdVal.toHash();
                PkVariantHash newId;
                for (const auto &iit : id) {
                    const PkString key2 = iit.first;
                    const PkVariant idVal = iit.second;
                    if (pkMapContains(idKeyList, key2)) {
                        newId[pkMapValue(idKeyList, key2)] = idVal;
                    } else {
                        newId[key2] = idVal;
                    }
                }
                newResource[PkString("/Identifier")] = newId;
            } else if (pkMapContains(keyList, key)) {
                newResource[pkMapValue(keyList, key)] = rdVal;
            } else {
                newResource[key] = rdVal;
            }
        }

        newResources.push_back(PkVariantHash({{PkString("/Resource"), newResource}}));
    }
    newObject[PkString("/Resources")] = newResources;
    return newObject;
}

PkVariantHash uncompressKeysDocumentResources(const PkVariantHash object) {
    PkVariantHash newObject;

    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;

         if (key == "/1") {
             newObject[PkString("/FontSet")] = uncompressKeysFontSet(val.toHash());
         } else if (key == "/2") {
             newObject[PkString("/MojiKumiCodeToClassSet")] = uncompressKeysMojiKumiCodeToClassSet(val.toHash());
         } else if (key == "/3") {
             newObject[PkString("/MojiKumiTableSet")] = uncompressKeysMojiKumiTableSet(val.toHash());
         } else if (key == "/4") {
             newObject[PkString("/KinsokuSet")] = uncompressKeysKinsokuSet(val.toHash());
         } else if (key == "/5") {
             newObject[PkString("/StyleSheetSet")] = uncompressKeysStyleSheetSet(val.toHash());
         } else if (key == "/6") {
             newObject[PkString("/ParagraphSheetSet")] = uncompressKeysParagraphSheetSet(val.toHash());
         } else if (key == "/8") {
             newObject[PkString("/TextFrameSet")] = uncompressKeysTextFrameSet(val.toHash());
         } else if (key == "/9") {
             newObject[PkString("/ListStyleSet")] = val;
         } else {
             newObject[key] = val;
         }
    }
    return newObject;
}

/*------- Document Objects ----------*/

PkVariantHash uncompressKeysTextModel(const PkVariantHash object) {
    PkVariantHash newObject;

    const std::map<PkString, PkString> runStyleKeyList {{"/0", "/Name"}, {"/5", "/Parent"}, {"/97", "/UUID"}};

    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;

         if (key == "/0") {
             newObject[PkString("/Text")] = val;
         } else if (key == "/5") {
             const PkVariantList array = pkHashValue(val.toHash(), PkString("/0")).toList();
             PkVariantList newArray;

             for (const PkVariant &run : array) {
                 const PkVariantHash runDataSheet = pkHashValue(pkHashValue(run.toHash(), PkString("/0")).toHash(), PkString("/0")).toHash();
                 PkVariantHash newDataSheet;

                 for (const auto &rit : runDataSheet) {
                     const PkString key2 = rit.first;
                     const PkVariant rdVal = rit.second;
                     if (key2 == "/5") {
                         newDataSheet[PkString("/Features")] = uncompressParagraphSheetFeatures(rdVal.toHash());
                     } else if (key2 == "/6") {
                         newDataSheet[PkString("/Parent")] = rdVal;
                     } else if (pkMapContains(runStyleKeyList, key)) {
                         // 原代码的 bug：contains 用外层 key（"/5"），value 用内层 key2。
                         newDataSheet[pkMapValue(runStyleKeyList, key2)] = rdVal;
                     } else {
                         newDataSheet[key2] = rdVal;
                     }
                 }

                 const PkVariantHash newSheet = {{PkString("/ParagraphSheet"), newDataSheet}};
                 PkVariantHash newRunData = {{PkString("/RunData"), newSheet}};
                 newRunData[PkString("/Length")] = pkHashValue(run.toHash(), PkString("/1"));
                 newArray.push_back(newRunData);
             }
             const PkVariantHash arrayParent = {{PkString("/RunArray"), newArray}};
             newObject[PkString("/ParagraphRun")] = arrayParent;
         } else if (key == "/6") {
             const PkVariantList array = pkHashValue(val.toHash(), PkString("/0")).toList();
             PkVariantList newArray;

             for (const PkVariant &run : array) {
                 const PkVariantHash runDataSheet = pkHashValue(pkHashValue(run.toHash(), PkString("/0")).toHash(), PkString("/0")).toHash();
                 PkVariantHash newDataSheet;

                 for (const auto &rit : runDataSheet) {
                     const PkString key2 = rit.first;
                     const PkVariant rdVal = rit.second;
                     if (key2 == "/6") {
                         newDataSheet[PkString("/Features")] = uncompressStyleSheetFeatures(rdVal.toHash());
                     } else if (key2 == "/5") {
                         newDataSheet[PkString("/Parent")] = rdVal;
                     } else if (pkMapContains(runStyleKeyList, key)) {
                         // 原代码的 bug：contains 用外层 key（"/6"，不在 runStyleKeyList），
                         // 所以这里实际走不到，与 else 分支行为一致。
                         newDataSheet[pkMapValue(runStyleKeyList, key2)] = rdVal;
                     } else {
                         newDataSheet[key2] = rdVal;
                     }
                 }

                 const PkVariantHash newSheet = {{PkString("/StyleSheet"), newDataSheet}};
                 PkVariantHash newRunData = {{PkString("/RunData"), newSheet}};
                 newRunData[PkString("/Length")] = pkHashValue(run.toHash(), PkString("/1"));
                 newArray.push_back(newRunData);
             }
             const PkVariantHash arrayParent = {{PkString("/RunArray"), newArray}};
             newObject[PkString("/StyleRun")] = arrayParent;
         } else if (key == "/10") {
             newObject[PkString("/StorySheet")] = val;
         } else {
             newObject[key] = val;
         }
    }
    return newObject;
}

PkVariantHash uncompressGlyphStrikeDef(const PkVariantHash object) {
    PkVariantHash newObject;

    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;
           // bounds,
         if (key == "/99") {
             newObject[PkString("/StreamTag")] = val;
         } else if (key == "/1") {
             newObject[PkString("/Bounds")] = val;
         } else if (key == "/5") {
             newObject[PkString("/Glyphs")] = val;
         } else if (key == "/10") {
             newObject[PkString("/GlyphAdjustments")] = val;
         } else if (key == "/8") {
             newObject[PkString("/VisualBounds")] = val;
         } else if (key == "/9") {
             newObject[PkString("/RenderedBounds")] = val;
         } else if (key == "/15") {
             newObject[PkString("/Invalidation")] = val;
         } else if (key == "/14") {
             newObject[PkString("/EndsInCR")] = val;
         } else if (key == "/12") {
             newObject[PkString("/SelectionAscent")] = val;
         } else if (key == "/13") {
             newObject[PkString("/SelectionDescent")] = val;
         } else {
             newObject[key] = val;
         }
    }
    return newObject;
}

PkVariantHash uncompressSegmentDef(const PkVariantHash object) {
    PkVariantHash newObject;

    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;

         if (key == "/99") {
             newObject[PkString("/StreamTag")] = val;
         } else if (key == "/1") {
             newObject[PkString("/Bounds")] = val;
         } else if (key == "/5") {
             newObject[PkString("/ChildProcession")] = val;
         } else if (key == "/6") {
             const PkVariantList array = val.toList();
             PkVariantList newArray;
             for (const PkVariant &entry : array) {
                 newArray.push_back(uncompressGlyphStrikeDef(entry.toHash()));
             }
             newObject[PkString("/Children")] = newArray;
         } else if (key == "/15") {
             newObject[PkString("/Mapping")] = val;
         }  else if (key == "/20") {
             newObject[PkString("/FirstCharacterIndexInSegment")] = val;
         } else {
             newObject[key] = val;
         }
    }
    return newObject;
}

PkVariantHash uncompressLineDef(const PkVariantHash object) {
    PkVariantHash newObject;

    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;

         if (key == "/99") {
             newObject[PkString("/StreamTag")] = val;
         } else if (key == "/0") {
             newObject[PkString("/Transform")] = val;
         } else if (key == "/1") {
             newObject[PkString("/Bounds")] = val;
         } else if (key == "/5") {
             newObject[PkString("/ChildProcession")] = val;
         } else if (key == "/6") {
             const PkVariantList array = val.toList();
             PkVariantList newArray;
             for (const PkVariant &entry : array) {
                    newArray.push_back(uncompressSegmentDef(entry.toHash()));
             }
             newObject[PkString("/Children")] = newArray;
         } else if (key == "/10") {
             newObject[PkString("/Baseline")] = val;
         } else if (key == "/14") {
             newObject[PkString("/SelectionAscent")] = val;
         } else if (key == "/15") {
             newObject[PkString("/SelectionDescent")] = val;
         } else {
             newObject[key] = val;
         }
    }
    return newObject;
}

PkVariantHash uncompressStrikeDef(const PkVariantHash object, bool flip = false) {
    PkVariantHash newObject;
    const PkString tag = pkHashContains(object, PkString("/99"))? pkHashValue(object, PkString("/99")).toString()
                                        : pkHashValue(object, PkString("/StreamTag")).toString();

    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;

         if (key == "/99") {
             newObject[PkString("/StreamTag")] = val;
         } else if ((key == "/0" && !flip) || (key == "/1" && flip)) {
             newObject[PkString("/Bounds")] = val;
         } else if ((key == "/1" && !flip) || (key == "/0" && flip)) {
             newObject[PkString("/Transform")] = val;
         } else if (key == "/5") {
             newObject[PkString("/ChildProcession")] = val;
         } else if (key == "/6") {
             const PkVariantList array = val.toList();
             PkVariantList newArray;
             for (const PkVariant &entry : array) {
                 const PkVariantHash eh = entry.toHash();
                 const PkString streamTag = pkHashContains(eh, PkString("/99"))? pkHashValue(eh, PkString("/99")).toString()
                                                                          : pkHashValue(eh, PkString("/StreamTag")).toString();
                 if (streamTag == "/PC" || streamTag == "/PathSelectGroupCharacter"
                         || streamTag == "/F" || streamTag == "/FrameStrike" || streamTag == "/RowColStrike" || streamTag == "/R") {
                    newArray.push_back(uncompressStrikeDef(eh, true));
                 } else if (streamTag == "/L" || streamTag == "/LineStrike") {
                     newArray.push_back(uncompressLineDef(eh));
                 }
             }
             newObject[PkString("/Children")] = newArray;
         } else if (key == "/10" && (tag == "/F" || tag == "/FrameStrike")) { // only for frame strike.
             newObject[PkString("/Frame")] = val;
         } else {
             newObject[key] = val;
         }
    }
    return newObject;
}

PkVariantHash uncompressKeysTextView(const PkVariantHash object) {
    PkVariantHash newObject;
    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;

         if (key == "/0") {
             const PkVariantList array = val.toList();
             PkVariantList newArray;
             for (const PkVariant &entry : array) {
                 const PkVariant resource = pkHashValue(entry.toHash(), PkString("/0"));

                newArray.push_back(PkVariantHash({{PkString("/Resource"), resource}}));
             }

             newObject[PkString("/Frames")] = newArray;
         } else if (key == "/2") {
             const PkVariantList array = val.toList();
             PkVariantList newArray;
             for (const PkVariant &entry : array) {
                 newArray.push_back(uncompressStrikeDef(entry.toHash()));
             }
             newObject[PkString("/Strikes")] = newArray;
         } else {
             newObject[key] = val;
         }
    }
    return newObject;
}

PkVariantHash uncompressKeysTextObject(const PkVariantHash object) {
    PkVariantHash newObject;

    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;

         if (key == "/0") {
             newObject[PkString("/Model")] = uncompressKeysTextModel(val.toHash());
         } else if (key == "/1") {
             newObject[PkString("/View")] = uncompressKeysTextView(val.toHash());
         } else {
             newObject[key] = val;
         }
    }
    return newObject;
}

PkVariantHash uncompressSmartQuoteSettings(const PkVariantHash object) {
    PkVariantHash newObject;
    const std::map<PkString, PkString> keyList {
        {"/0", "/Language"},
        {"/1", "/OpenDoubleQuote"},
        {"/2", "/CloseDoubleQuote"},
        {"/3", "/OpenSingleQuote"},
        {"/4", "/CloseSingleQuote"},

    };

    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;
        if (pkMapContains(keyList, key)) {
            newObject[pkMapValue(keyList, key)] = val;
        } else {
            newObject[key] = val;
        }
    }
    return newObject;
}

PkVariantHash uncompressHiddenGlyphSettings(const PkVariantHash object) {
    PkVariantHash newObject;

    if (pkHashContains(object, PkString("/0"))) {
        newObject[PkString("/AlternateGlyphFont")] = pkHashValue(object, PkString("/0"));
    }
    if (pkHashContains(object, PkString("/1"))) {
        const PkVariantList array = pkHashValue(object, PkString("/1")).toList();
        PkVariantList newArray;
        for (const PkVariant &entry : array) {
            // 原代码的 bug：newEntry 恒为空，/WhitespaceCharacter 与 /AlternateCharacter
            // 读到的都是 Invalid null；且 append 的是整个 newObject 而不是 newEntry。
            PkVariantHash newEntry;
            newObject[PkString("/WhitespaceCharacter")] = pkHashValue(newEntry, PkString("/0"));
            newObject[PkString("/AlternateCharacter")] = pkHashValue(newEntry, PkString("/1"));
            newArray.push_back(newObject);
        }
        newObject[PkString("/WhitespaceCharacterMapping")] = newArray;
    }

    return newObject;
}

PkVariantHash uncompressKeysDocumentSettings(const PkVariantHash object) {
    PkVariantHash newObject;
    const std::map<PkString, PkString> keyList {
        //{"/0", "/HiddenGlyphFont"},
        {"/1", "/NormalStyleSheet"},
        {"/2", "/NormalParagraphSheet"},
        {"/3", "/SuperscriptSize"},
        {"/4", "/SuperscriptPosition"},

        {"/5", "/SubscriptSize"},
        {"/6", "/SubscriptPosition"},
        {"/7", "/SmallCapSize"},
        {"/8", "/UseSmartQuotes"},
        //{"/9", "/SmartQuoteSets"},

        //{"/10", ""},
        {"/11", "/GreekingSize"},
        //{"/12", ""},
        //{"/13", ""},
        //{"/14", ""},

        {"/15", "/LinguisticSettings"},
        {"/16", "/UseSmartLists"},
        {"/17", "/DefaultStoryDir"},
    };

    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;
        if (key == "/0") {
            newObject[PkString("/HiddenGlyphFont")] = uncompressHiddenGlyphSettings(val.toHash());
        } else if (key == "/9") {
            const PkVariantList array = val.toList();
            PkVariantList newArray;
            for (const PkVariant &entry : array) {
                newArray.push_back(uncompressSmartQuoteSettings(entry.toHash()));
            }
            newObject[PkString("/SmartQuoteSets")] = newArray;
        } else if (pkMapContains(keyList, key)) {
            newObject[pkMapValue(keyList, key)] = val;
        } else {
            newObject[key] = val;
        }
    }

    return newObject;
}

PkVariantHash uncompressKeysDocumentObjects(const PkVariantHash object) {
    PkVariantHash newObject;
    for (const auto &it : object) {
        const PkString key = it.first;
        const PkVariant val = it.second;

         if (key == "/0") {
             newObject[PkString("/DocumentSettings")] = uncompressKeysDocumentSettings(val.toHash());
         } else if (key == "/1") {
             const PkVariantList array = val.toList();
             PkVariantList newArray;
             for (const PkVariant &entry : array) {
                 newArray.push_back(uncompressKeysTextObject(entry.toHash()));
             }
             newObject[PkString("/TextObjects")] = newArray;
         } else if (key == "/2") {
             newObject[PkString("/OriginalNormalStyleFeatures")] = uncompressStyleSheetFeatures(val.toHash());
         } else if (key == "/3") {
             newObject[PkString("/OriginalNormalParagraphFeatures")] = uncompressParagraphSheetFeatures(val.toHash());
         } else {
             newObject[key] = val;
         }
    }

    return newObject;
}

PkVariantHash KisTxt2Utils::uncompressKeys(PkVariantHash doc)
{

    PkVariantHash newDoc;
    for (const auto &it : doc) {
        const PkString key = it.first;
        const PkVariant val = it.second;
        if (key == "/0") {
            newDoc[PkString("/DocumentResources")] = uncompressKeysDocumentResources(val.toHash());
        } else if (key == "/1") {
            newDoc[PkString("/DocumentObjects")] = uncompressKeysDocumentObjects(val.toHash());
        } else {
            newDoc[key] = val;
        }
    }
    return newDoc;
}

//-------------- Default Txt2 ----------------//

static PkVariantHash defaultFill {
    { "/StreamTag", "/SimplePaint"},
    {
        "/Color", PkVariantHash{{"/Type", 1}, {"/Values", PkVariantList({1.0, 0.0, 0.0, 0.0})}}
    }
};

static PkVariantHash defaultBGFill {
    { "/StreamTag", "/SimplePaint"},
    {
        "/Color", PkVariantHash{{"/Type", 1}, {"/Values", PkVariantList({1.0, 1.0, 1.0, 0.0})}}
    }
};

static PkVariantHash defaultStyle {
    {"/Font", 1},
    {"/FontSize", 12.0},
    {"/FauxBold", false},
    {"/FauxItalic", false},
    {"/AutoLeading", true},

    {"/Leading", 0.0},
    {"/HorizontalScale", 1.0},
    {"/VerticalScale", 1.0},
    {"/Tracking", 0},
    {"/BaselineShift", 0.0},

    {"/CharacterRotation", 0.0},
    {"/AutoKern", 1},
    {"/FontCaps", 0},
    {"/FontBaseline", 0},
    {"/FontOTPosition", 0},

    {"/StrikethroughPosition", 0},
    {"/UnderlinePosition", 0},
    {"/UnderlineOffset", 0.0},
    {"/Ligatures", true},
    {"/DiscretionaryLigatures", false},

    {"/ContextualLigatures", false},
    {"/AlternateLigatures", false},
    {"/OldStyle", false},
    {"/Fractions", false},
    {"/Ordinals", false},

    {"/Swash", false},
    {"/Titling", false},
    {"/ConnectionForms", false},
    {"/StylisticAlternates", false},
    {"/Ornaments", false},

    {"/FigureStyle", 0},
    {"/ProportionalMetrics", false},
    {"/Kana", false},
    {"/Italics", false},
    {"/Ruby", false},

    {"/BaselineDirection", 2},
    {"/Tsume", 0.0},
    {"/StyleRunAlignment", 0},
    {"/Language", 0},
    {"/JapaneseAlternateFeature", 0},

    {"/EnableWariChu", false},
    {"/WariChuLineCount", 2},
    {"/WariChuLineGap", 0},
    {"/WariChuSubLineAmount", PkVariantHash({{"/WariChuSubLineScale", 0.5}})},
    {"/WariChuWidowAmount", 2},

    {"/WariChuOrphanAmount", 2},
    {"/WariChuJustification", 7},
    {"/TCYUpDownAdjustment", 0},
    {"/TCYLeftRightAdjustment", 0},
    {"/LeftAki", -1.0},

    {"/RightAki", -1.0},
    {"/JiDori", 0},
    {"/NoBreak", false},
    {"/FillColor", defaultFill},
    {"/StrokeColor", defaultFill},

    {"/Blend",  PkVariantHash({{"/StreamTag", "/SimpleBlender"}})},
    {"/FillFlag", true},
    {"/StrokeFlag", false},
    {"/FillFirst", false},

    {"/FillOverPrint", false},
    {"/StrokeOverPrint", false},
    {"/LineCap", 0},
    {"/LineJoin", 0},
    {"/LineWidth", 1.0},
    {"/MiterLimit", 4.0},

    {"/LineDashOffset", 0.0},
    {"/LineDashArray", PkVariantList()},
    {"/Type", PkVariantList()},
    {"/Kashidas", 0},
    {"/DirOverride", 0},

    {"/DigitSet", 0},
    {"/DiacVPos", 0},
    {"/DiacXOffset", 0.0},
    {"/DiacYOffset", 0.0},
    {"/OverlapSwash", false},

    {"/JustificationAlternates", false},
    {"/StretchedAlternates", false},
    {"/FillVisibleFlag", true},
    {"/StrokeVisibleFlag", true},
    {"/FillBackgroundColor", defaultBGFill},

    {"/FillBackgroundFlag", false},
    {"/UnderlineStyle", 0},
    {"/DashedUnderlineGapLength", 3.0},
    {"/DashedUnderlineDashLength", 3.0},
    {"/SlashedZero", false},

    {"/StylisticSets", 0},
    {"/CustomFeature", PkVariantHash({{"/StreamTag", "/SimpleCustomFeature"}})},
    {"/MarkYDistFromBaseline", 100.0},
};

static PkVariantHash defaultParagraph {
    {"/Justification", 0},
    {"/FirstLineIndent", 0.0},
    {"/StartIndent", 0.0},
    {"/EndIndent", 0.0},
    {"/SpaceBefore", 0.0},

    {"/SpaceAfter", 0.0},
    {"/DropCaps", 1},
    {"/AutoLeading", 1.2},
    {"/LeadingType", 0},
    {"/AutoHyphenate", true},

    {"/HyphenatedWordSize", 6},
    {"/PreHyphen", 2},
    {"/PostHyphen", 2},
    {"/ConsecutiveHyphens", 0},
    {"/Zone", 36.0},

    {"/HyphenateCapitalized", true},
    {"/HyphenationPreference", 0.5},
    {"/WordSpacing", PkVariantList({0.8, 1.0, 1.3})},
    {"/LetterSpacing", PkVariantList({0.0, 0.0, 0.0})},
    {"/GlyphSpacing", PkVariantList({1.0, 1.0, 1.0})},

    {"/SingleWordJustification", 6},
    {"/Hanging", false},
    {"/AutoTCY", 0},
    {"/KeepTogether", true},
    {"/BurasagariType", 0},

    {"/KinsokuOrder", 0},
    {"/KurikaeshiMojiShori", false},
    {"/Kinsoku", "/nil"},
    {"/MojiKumiTable", "/nil"},
    {"/EveryLineComposer", false},

    {"/TabStops", PkVariantHash()},
    {"/DefaultTabWidth", 36.0},
    {"/DefaultStyle", PkVariantHash()},
    {"/ParagraphDirection", 0},
    {"/JustificationMethod", 7},

    {"/ComposerEngine", 1},
    {"/ListStyle", "/nil"},
    {"/ListTier", 0},
    {"/ListSkip", false},
    {"/ListOffset", 0},

    {"/KashidaWidth", 2}
};

static PkVariantHash kinsokuNone {
    {"/NoStart", ""},
    {"/NoEnd", ""},
    {"/Keep", ""},
    {"/Hanging", ""},
    {"/PredefinedTag", 0}
};

static PkVariantHash kinsokuHard {
    {"/NoStart", "!),.:;?]}¢—’”‰℃℉、。々〉》」』】〕ぁぃぅぇぉっゃゅょゎ゛゜ゝゞァィゥェォッャュョヮヵヶ・ーヽヾ！％），．：；？］｝"},
    {"/NoEnd", "([{£§‘“〈《「『【〒〔＃＄（＠［｛￥"},
    {"/Keep", "—‥…"},
    {"/Hanging", "、。，．"},
    {"/PredefinedTag", 1}
};

static PkVariantHash kinsokuSoft {
    {"/NoStart", "’”、。々〉》」』】〕ゝゞ・ヽヾ！），．：；？］｝"},
    {"/NoEnd", "‘“〈《「『【〔（［｛"},
    {"/Keep", "—‥…"},
    {"/Hanging", "、。，．"},
    {"/PredefinedTag", 2}
};

PkVariantHash KisTxt2Utils::defaultTxt2()
{
    PkVariantHash doc;

    PkVariantHash documentResources;
    PkVariantHash documentObjects;

    PkVariantHash fontSet;
    PkVariantList fontResources;

    PkVariantHash fontInvis {
        {"/Name", "AdobeInvisFont"},
        {"/Type", 0}
    };

    PkVariantHash fontMyriad {
        {"/Name", "MyriadPro-Regular"},
        {"/Type", 0},
        {"/Version", "Version 2.115;PS 2.000;hotconv 1.0.81;makeotf.lib2.5.63406"}
    };
    PkVariantHash fR = {
        {"/Resource", PkVariantHash({{"/Identifier", fontMyriad}, {"/StreamTag", "/CoolTypeFont"}})}
    };
    fontResources.push_back(fR);
    fR = {
        {"/Resource", PkVariantHash({{"/Identifier", fontInvis}, {"/StreamTag", "/CoolTypeFont"}})}
    };
    fontResources.push_back(fR);

    fontSet[PkString("/Resources")] = fontResources;
    documentResources[PkString("/FontSet")] = fontSet;
    PkVariantHash kinsokuSet;
    PkVariantList kinsokuResources;
    PkVariantList kinsokuDisplay;
    PkVariantHash kin =  PkVariantHash ({{"/Resource", PkVariantHash({ {"/Name", "None"}, {"/Data", kinsokuNone}})}});
    kinsokuResources.push_back(kin);
    kinsokuDisplay.push_back(PkVariantHash({{"/Resource", 0}}));
    kin =  PkVariantHash ({{"/Resource", PkVariantHash({ {"/Name", "PhotoshopKinsokuHard"}, {"/Data", kinsokuHard}})}});
    kinsokuResources.push_back(kin);
    kinsokuDisplay.push_back(PkVariantHash({{"/Resource", 1}}));
    kin =  PkVariantHash ({{"/Resource", PkVariantHash({ {"/Name", "PhotoshopKinsokuSoft"}, {"/Data", kinsokuSoft}})}});
    kinsokuResources.push_back(kin);
    kinsokuDisplay.push_back(PkVariantHash({{"/Resource", 2}}));
    kin =  PkVariantHash ({{"/Resource", PkVariantHash({ {"/Name", "Hard"}, {"/Data", kinsokuHard}})}});
    kinsokuResources.push_back(kin);
    kinsokuDisplay.push_back(PkVariantHash({{"/Resource", 3}}));
    kin =  PkVariantHash ({{"/Resource", PkVariantHash({ {"/Name", "Soft"}, {"/Data", kinsokuSoft}})}});
    kinsokuResources.push_back(kin);
    kinsokuDisplay.push_back(PkVariantHash({{"/Resource", 4}}));
    kinsokuSet[PkString("/Resources")] = kinsokuResources;
    kinsokuSet[PkString("/DisplayList")] = kinsokuDisplay;
    documentResources[PkString("/KinsokuSet")] = kinsokuSet;

    //PkVariantHash listStyleSet;
    //documentResources.insert("/ListStyleSet", listStyleSet);
    PkVariantHash mojiKumiCodeToClassSet = {
        {"/Resources", PkVariantList({
             PkVariantHash({
                 {"/Resource", PkVariantHash({
                      {"/Name", ""}
                  })}
             })
         })},
        {"/DisplayList", PkVariantList({PkVariantHash({{"/Resource", 0}})})}
    };
    documentResources[PkString("/MojiKumiCodeToClassSet")] = mojiKumiCodeToClassSet;

    // 旧哈希的迭代顺序本就不保证；Pk 侧用 std::map（按 key 排序）得到确定性顺序。
    std::map<PkString, int> mojilist {
        {"Photoshop6MojiKumiSet4", 2},
        {"Photoshop6MojiKumiSet3", 4},
        {"Photoshop6MojiKumiSet2", 3},
        {"Photoshop6MojiKumiSet1", 1},
        {"YakumonoHankaku", 1},
        {"GyomatsuYakumonoHankaku", 3},
        {"GyomatsuYakumonoZenkaku", 4},
        {"YakumonoZenkaku", 2},
    };

    PkVariantHash mojiKumiTableSet;

    PkVariantList mojiResources;
    PkVariantList mojiDisplay;

    for (const auto &mojiIt : mojilist) {
        const PkString key = mojiIt.first;
        mojiDisplay.push_back(PkVariantHash({{"/Resource", (long long)mojiDisplay.size()}}));
        PkVariantHash mojikumiTable = {
            {"/Name", key},
            {"/Members", PkVariantHash({
                 {"/CodeToClass", 0},
                 {"/PredefinedTag", mojiIt.second}

             })
            },
        };

        mojiResources.push_back(mojikumiTable);
    }

    mojiKumiTableSet[PkString("/Resources")] = mojiResources;
    mojiKumiTableSet[PkString("/DisplayList")] = mojiDisplay;
    documentResources[PkString("/MojiKumiTableSet")] = mojiKumiTableSet;

    PkVariantHash paragraphSheetSet = {
        {"/Resources", PkVariantList({
             PkVariantHash({
                 {"/Resource", PkVariantHash({
                      {"/Name", "Normal RGB"},
                      {"/Features", defaultParagraph}
                  })}
             })
         })},
        {"/DisplayList", PkVariantList({PkVariantHash({{"/Resource", 0}})})}
    };
    documentResources[PkString("/ParagraphSheetSet")] = paragraphSheetSet;
    PkVariantHash styleSheetSet = {
        {"/Resources", PkVariantList({
             PkVariantHash({
                 {"/Resource", PkVariantHash({
                      {"/Name", "Normal RGB"},
                      {"/Features", defaultStyle}
                  })}
             })
         })},
        {"/DisplayList", PkVariantList({PkVariantHash({{"/Resource", 0}})})}
    };
    documentResources[PkString("/StyleSheetSet")] = styleSheetSet;

    PkVariantHash textFrameSet;
    textFrameSet[PkString("/Resources")] = PkVariantList();
    documentResources[PkString("/TextFrameSet")] = textFrameSet;

    // Document settings ----------
    PkVariantHash DocumentSettings;
    DocumentSettings[PkString("/DefaultStoryDir")] = 0;
    // smart quote?
    // alternate glyph?
    DocumentSettings[PkString("/SubscriptPosition")] = 0.333;
    DocumentSettings[PkString("/SubscriptSize")] = 0.583;
    DocumentSettings[PkString("/SuperscriptPosition")] = 0.333;
    DocumentSettings[PkString("/SuperscriptSize")] = 0.583;
    DocumentSettings[PkString("/SmallCapSize")] = 0.7;
    DocumentSettings[PkString("/NormalParagraphSheet")] = 0;
    DocumentSettings[PkString("/NormalStyleSheet")] = 0;
    documentObjects[PkString("/DocumentSettings")] = DocumentSettings;
    const PkVariantHash OriginalNormalParagraphFeatures = defaultParagraph;
    documentObjects[PkString("/OriginalNormalParagraphFeatures")] = OriginalNormalParagraphFeatures;
    const PkVariantHash OriginalNormalStyleFeatures = defaultStyle;
    documentObjects[PkString("/OriginalNormalStyleFeatures")] = OriginalNormalStyleFeatures;
    PkVariantList TextObjects;
    documentObjects[PkString("/TextObjects")] = TextObjects;

    doc[PkString("/DocumentResources")] = documentResources;
    doc[PkString("/DocumentObjects")] = documentObjects;
    return doc;
}

PkVariantHash defaultGrid {
    {"/GridIsOn", false},
    {"/ShowGrid", false},
    {"/GridSize", 18.0},
    {"/GridLeading", 22.0},
    {"/GridColor", PkVariantHash{{"/Type", 1}, {"/Values", PkVariantList({0.0, 0.0, 0.0, 1.0})}}},
    {"/GridLeadingFillColor", PkVariantHash{{"/Type", 1}, {"/Values", PkVariantList({0.0, 0.0, 0.0, 1.0})}}},
    {"/AlignLineHeightToGridFlags", false},
};

static PkStringList simpleStyleAllowed {
    "/Font",
    "/FontSize",
    "/FauxBold",
    "/FauxItalic",
    "/AutoLeading",

    "/Leading",
    "/HorizontalScale",
    "/VerticalScale",
    "/Tracking",
    "/BaselineShift",

    "/FontCaps",
    "/FontBaseline",
    "/Ligatures",
    "/BaselineDirection",
    "/Tsume",

    "/StyleRunAlignment",
    "/Language",
    "/NoBreak",
    "/FillFlag",
    "/StrokeFlag",

    "/FillFirst",
    "/Kashida",
};
static PkVariantHash simplifyStyleSheet(const PkVariantHash complex) {
    PkVariantHash simple;
    for (const auto &it : complex) {
        const PkString key = it.first;
        const PkVariant val = it.second;
        if (simpleStyleAllowed.contains(key)) {
            simple[key] = val;
        }else if (key == "/StrokeColor" || key == "/FillColor") {
            simple[key] = pkHashValue(val.toHash(), PkString("/Color"));
        } else if (key == "/UnderlinePosition") {
            const bool bval = val.toBool();
            simple[PkString("/Underline")] = bval;
            if (val.toInt() == 2) {
                simple[PkString("/YUnderline")] = 0;
            } else {
                simple[PkString("/YUnderline")] = 1;
            }
        } else if (key == "/StrikethroughPosition") {
            const bool bval = val.toBool();
            simple[PkString("/Strikethrough")] = bval;
        } else if (key == "/AutoKerning") {
            const bool bval = val.toBool();
            simple[PkString("/AutoKern")] = bval;
        } else if (key == "/LineWidth") {
            simple[PkString("/OutlineWidth")] = val;
        } else if (key == "/DiscretionaryLigatures") {
            simple[PkString("/DLigatures")] = val;
        }
    }
    simple[PkString("/Kerning")] = 0.0;
    simple[PkString("/HindiNumbers")] = false;
    simple[PkString("/DiacriticPos")] = 2;
    return simple;
}
static PkStringList simpleParagraphAllowed {
    "/Justification",
    "/FirstLineIndent",
    "/StartIndent",
    "/EndIndent",
    "/SpaceBefore",

    "/AutoHyphenate",
    "/HyphenatedWordSize",
    "/PreHyphen",
    "/PostHyphen",
    "/Zone",

    "/WordSpacing",
    "/LetterSpacing",
    "/GlyphSpacing",
    "/SpaceAfter",
    "/AutoLeading",

    "/LeadingType",
    "/Hanging",
    "/Burasagari",
    "/KinsokuOrder",
    "/EveryLineComposer",
};
static PkVariantHash simplifyParagraphSheet(const PkVariantHash complex) {
    PkVariantHash simple;
    for (const auto &it : complex) {
        const PkString key = it.first;
        if (simpleParagraphAllowed.contains(key)) {
            simple[key] = it.second;
        }
    }
    return simple;
}

PkVariantHash KisTxt2Utils::tyShFromTxt2(const PkVariantHash Txt2, const PkRectF boundsInPx, int textIndex)
{
    PkVariantHash tySh;

    const PkVariantHash documentObjects = pkHashValue(Txt2, PkString("/DocumentObjects")).toHash();
    const PkVariantList textObjects = pkHashValue(documentObjects, PkString("/TextObjects")).toList();
    PkVariantHash textObject;
    if (textIndex < (int)textObjects.size()) {
        textObject = pkListValue(textObjects, static_cast<size_t>(textIndex)).toHash();
    }


    const PkVariantHash model = pkHashValue(textObject, PkString("/Model")).toHash();
    const PkVariantHash view = pkHashValue(textObject, PkString("/View")).toHash();

    const PkVariantHash documentResources = pkHashValue(Txt2, PkString("/DocumentResources")).toHash();
    const PkVariantList textFrames = pkHashValue(pkHashValue(documentResources, PkString("/TextFrameSet")).toHash(), PkString("/Resources")).toList();

    PkVariantHash engineDict;

    PkVariantHash editor;
    editor[PkString("/Text")] = pkHashValue(model, PkString("/Text"));
    engineDict[PkString("/Editor")] = editor;


    engineDict[PkString("/GridInfo")] = defaultGrid;

    // paragraph
    const PkVariantHash para = pkHashValue(model, PkString("/ParagraphRun")).toHash();
    const PkVariantList paraRunArray = pkHashValue(para, PkString("/RunArray")).toList();
    // if we want to go over each entry, here's the moment to do so.
    const int length = pkHashValue(pkListValue(paraRunArray, 0).toHash(), PkString("/Length")).toInt();
    const PkVariantHash paraSheet = pkHashValue(pkHashValue(pkListValue(paraRunArray, 0).toHash(), PkString("/RunData")).toHash(), PkString("/ParagraphSheet")).toHash();
    const PkVariantHash paragraphStyle = simplifyParagraphSheet(pkHashValue(paraSheet, PkString("/Features")).toHash());
    PkVariantHash paragraphRun;
    paragraphRun[PkString("/RunLengthArray")] = PkVariantList({PkVariant(length)});
    PkVariantHash paragraphAdjustments  = PkVariantHash {
        {"/Axis", PkVariantList({1.0, 0.0, 1.0})},
        {"/XY", PkVariantList({0.0, 0.0})}
    };
    paragraphRun[PkString("/RunArray")] = PkVariantList({ PkVariantHash{
        {"/ParagraphSheet", paragraphStyle},
        {"/Adjustments", paragraphAdjustments}
    }});
    paragraphRun[PkString("/IsJoinable")] = 1; //no idea what this means.
    paragraphRun[PkString("/DefaultRunData")] = PkVariantHash{
        {"/ParagraphSheet",
                PkVariantHash{{"/DefaultStyleSheet", 0},
                            {"/Properties", PkVariantHash()}} },
        {"/Adjustments", paragraphAdjustments}};

    engineDict[PkString("/ParagraphRun")] = paragraphRun;

    const PkVariantHash txt2StyleRun = pkHashValue(model, PkString("/StyleRun")).toHash();
    const PkVariantList txt2RunArray = pkHashValue(txt2StyleRun, PkString("/RunArray")).toList();
    PkVariantHash styleRun;

    PkVariantList properStyleRun;
    PkVariantList styleRunArray;
    for (const PkVariant &entry : txt2RunArray) {
        PkVariantHash properStyle;
        const PkVariantHash fea = pkHashValue(pkHashValue(pkHashValue(entry.toHash(), PkString("/RunData")).toHash(), PkString("/StyleSheet")).toHash(), PkString("/Features")).toHash();
        properStyle[PkString("/StyleSheetData")] = simplifyStyleSheet(fea);
        PkVariantHash s;
        s[PkString("/StyleSheet")] = properStyle;
        properStyleRun.push_back(s);
        styleRunArray.push_back(pkHashValue(entry.toHash(), PkString("/Length")).toInt());
    }
    styleRun[PkString("/RunArray")] = properStyleRun;
    styleRun[PkString("/RunLengthArray")] = styleRunArray;
    styleRun[PkString("/IsJoinable")] = 2;

    styleRun[PkString("/DefaultRunData")] = PkVariantHash{{"/StyleSheet", PkVariantHash{{"/StyleSheetData", PkVariantHash()}} }};
    engineDict[PkString("/StyleRun")] = styleRun;
    // rendered data...

    const int frameIndex = pkHashValue(pkListValue(pkHashValue(view, PkString("/Frames")).toList(), 0).toHash(), PkString("/Resource")).toInt();
    const PkVariantHash textFrame = pkHashValue(pkListValue(textFrames, static_cast<size_t>(frameIndex)).toHash(), PkString("/Resource")).toHash();
    const PkVariantHash textFrameData = pkHashValue(textFrame, PkString("/Data")).toHash();
    PkVariantHash rendered;
    int shapeType = pkHashValueDefault(textFrameData, PkString("/Type"), PkVariant(0)).toInt(); // 0 point, 1 paragraph, 2, text on path
    int writingDirection = pkHashValueDefault(textFrameData, PkString("/LineOrientation"), PkVariant(0)).toInt();
    PkVariantHash photoshop = PkVariantHash {{"/ShapeType", std::max(1, shapeType)},
        {"/TransformPoint0", PkVariantList({1.0, 0.0})},
        {"/TransformPoint1", PkVariantList({0.0, 1.0})},
        {"/TransformPoint2", PkVariantList({0.0, 0.0})}};
    if (shapeType == 0) {
        photoshop[PkString("/PointBase")] = PkVariantList({0.0, 0.0});
    } else if (shapeType == 1) {
        // this is the bounding box of the paragraph shape.
        photoshop[PkString("/BoxBounds")] = PkVariantList({0.0, 0.0, boundsInPx.width(), boundsInPx.height()});
    }
    PkVariantHash renderChild = PkVariantHash{
        {"/ShapeType", shapeType},
        {"/Procession", 0},
        {"/Lines", PkVariantHash{{"/WritingDirection", writingDirection}, {"/Children", PkVariantList()}}},
        {"/Cookie", PkVariantHash{{"/Photoshop", photoshop}}}};
    rendered[PkString("/Version")] = 1;
    rendered[PkString("/Shapes")] = PkVariantHash{{"/WritingDirection", writingDirection}, {"/Children", PkVariantList({renderChild})}};

    engineDict[PkString("/Rendered")] = rendered;

    // storysheet
    const PkVariantHash storySheet = pkHashValue(textObject, PkString("/StorySheet")).toHash();
    engineDict[PkString("/UseFractionalGlyphWidths")] = pkHashValueDefault(storySheet, PkString("/UseFractionalGlyphWidths"), PkVariant(true)).toBool();
    engineDict[PkString("/AntiAlias")] = pkHashValueDefault(storySheet, PkString("/AntiAlias"), PkVariant(1)).toInt();

    PkVariantHash resourceDict;

    const PkVariantList docFontSet = pkHashValue(pkHashValue(documentResources, PkString("/FontSet")).toHash(), PkString("/Resources")).toList();
    PkVariantList fontSet;
    for (const PkVariant &entry : docFontSet) {
        const PkVariantHash docFont = pkHashValue(entry.toHash(), PkString("/Resource")).toHash();
        PkVariantHash font = pkHashValue(docFont, PkString("/Identifier")).toHash();
        font[PkString("/FontType")] = pkHashValue(font, PkString("/Type"));
        font.erase(PkString("/Version"));
        font.erase(PkString("/Type"));
        font[PkString("/Script")] = 0;
        font[PkString("/Synthetic")] = 0;
        fontSet.push_back(font);
    }
    resourceDict[PkString("/FontSet")] = fontSet;

    const PkVariantHash documentSettings = pkHashValue(documentObjects, PkString("/DocumentSettings")).toHash();

    PkVariantHash kinHard = kinsokuHard;
    kinHard.erase(PkString("/PredefinedTag"));
    kinHard[PkString("/Name")] = "PhotoshopKinsokuHard";
    PkVariantHash kinSoft = kinsokuSoft;
    kinSoft.erase(PkString("/PredefinedTag"));
    kinSoft[PkString("/Name")] = "PhotoshopKinsokuSoft";
    resourceDict[PkString("/KinsokuSet")] = PkVariantList({kinHard, kinSoft});
    resourceDict[PkString("/MojiKumiSet")] = PkVariantList( {PkVariantHash{{"/InternalName", "Photoshop6MojiKumiSet1"}},
                                               PkVariantHash{{"/InternalName", "Photoshop6MojiKumiSet2"}},
                                               PkVariantHash{{"/InternalName", "Photoshop6MojiKumiSet3"}},
                                               PkVariantHash{{"/InternalName", "Photoshop6MojiKumiSet4"}}
                                              });
    resourceDict[PkString("/SubscriptPosition")] = pkHashValue(documentSettings, PkString("/SubscriptPosition"));
    resourceDict[PkString("/SubscriptSize")] = pkHashValue(documentSettings, PkString("/SubscriptSize"));
    resourceDict[PkString("/SuperscriptPosition")] = pkHashValue(documentSettings, PkString("/SuperscriptPosition"));
    resourceDict[PkString("/SuperscriptSize")] = pkHashValue(documentSettings, PkString("/SuperscriptSize"));
    resourceDict[PkString("/SmallCapSize")] = pkHashValue(documentSettings, PkString("/SmallCapSize"));
    resourceDict[PkString("/TheNormalParagraphSheet")] = pkHashValue(documentSettings, PkString("/NormalParagraphSheet"));
    resourceDict[PkString("/TheNormalStyleSheet")] = pkHashValue(documentSettings, PkString("/NormalStyleSheet"));

    const PkVariantList docStyleSheetSets = pkHashValue(pkHashValue(documentResources, PkString("/StyleSheetSet")).toHash(), PkString("/Resources")).toList();
    PkVariantList resourceStyleSheetList;
    for (const PkVariant &entry : docStyleSheetSets) {
        const PkVariantHash styleSheet = pkHashValue(entry.toHash(), PkString("/Resource")).toHash();
        PkVariantHash newSheet;
        newSheet[PkString("/Name")] = pkHashValue(styleSheet, PkString("/Name"));
        newSheet[PkString("/StyleSheetData")] = simplifyStyleSheet(pkHashValue(styleSheet, PkString("/Features")).toHash());
        resourceStyleSheetList.push_back(newSheet);
    }

    const PkVariantList docParagraphSheetSets = pkHashValue(pkHashValue(documentResources, PkString("/ParagraphSheetSet")).toHash(), PkString("/Resources")).toList();
    PkVariantList resourceParagraphSheetList;
    for (const PkVariant &entry : docParagraphSheetSets) {
        const PkVariantHash styleSheet = pkHashValue(entry.toHash(), PkString("/Resource")).toHash();
        PkVariantHash newSheet;
        newSheet[PkString("/Name")] = pkHashValue(styleSheet, PkString("/Name"));
        newSheet[PkString("/Properties")] = simplifyParagraphSheet(pkHashValue(styleSheet, PkString("/Features")).toHash());
        newSheet[PkString("/DefaultStyleSheet")] = 0;
        resourceParagraphSheetList.push_back(newSheet);
    }

    resourceDict[PkString("/StyleSheetSet")] = resourceStyleSheetList;
    resourceDict[PkString("/ParagraphSheetSet")] = resourceParagraphSheetList;

    tySh[PkString("/EngineDict")] = engineDict;
    tySh[PkString("/DocumentResources")] = resourceDict;
    tySh[PkString("/ResourceDict")] = resourceDict;
    return tySh;
}
