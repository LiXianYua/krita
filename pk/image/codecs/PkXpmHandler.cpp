/****************************************************************************
**
** Copyright (C) 2016 The Qt Company Ltd.
** Contact: https://www.qt.io/licensing/
**
** This file is part of the QtGui module of the Qt Toolkit.
**
** $QT_BEGIN_LICENSE:LGPL$
** Commercial License Usage
** Licensees holding valid commercial Qt licenses may use this file in
** accordance with the commercial license agreement provided with the
** Software or, alternatively, in accordance with the terms contained in
** a written agreement between you and The Qt Company. For licensing terms
** and conditions see https://www.qt.io/terms-conditions. For further
** information use the contact form at https://www.qt.io/contact-us.
**
** GNU Lesser General Public License Usage
** Alternatively, this file may be used under the terms of the GNU Lesser
** General Public License version 3 as published by the Free Software
** Foundation and appearing in the file LICENSE.LGPL3 included in the
** packaging of this file. Please review the following information to
** ensure the GNU Lesser General Public License version 3 requirements
** will be met: https://www.gnu.org/licenses/lgpl-3.0.html.
**
** GNU General Public License Usage
** Alternatively, this file may be used under the terms of the GNU
** General Public License version 2.0 or (at your option) the GNU General
** Public license version 3 or any later version approved by the KDE Free
** Qt Foundation. The licenses are as published by the Free Software
** Foundation and appearing in the file LICENSE.GPL2 and LICENSE.GPL3
** included in the packaging of this file. Please review the following
** information to ensure the GNU General Public License requirements will
** be met: https://www.gnu.org/licenses/gpl-2.0.html and
** https://www.gnu.org/licenses/gpl-3.0.html.
**
** $QT_END_LICENSE$
**
****************************************************************************/

#include "../PkImageFileDecoder.h"

#include <algorithm>
#include <cctype>
#include <cstdint>
#include <iterator>
#include <sstream>
#include <string>
#include <unordered_map>
#include <vector>

namespace
{

#define PK_XPM_RGB(r, g, b) \
    (0xFF000000u | (static_cast<uint32_t>(r) << 16) | \
     (static_cast<uint32_t>(g) << 8) | static_cast<uint32_t>(b))

static constexpr struct XpmRgbData {
    uint32_t value;
    const char name[21];
} kXpmRgbTable[] = {
    { PK_XPM_RGB(240,248,255),  "aliceblue" },
    { PK_XPM_RGB(250,235,215),  "antiquewhite" },
    { PK_XPM_RGB(255,239,219),  "antiquewhite1" },
    { PK_XPM_RGB(238,223,204),  "antiquewhite2" },
    { PK_XPM_RGB(205,192,176),  "antiquewhite3" },
    { PK_XPM_RGB(139,131,120),  "antiquewhite4" },
    { PK_XPM_RGB(127,255,212),  "aquamarine" },
    { PK_XPM_RGB(127,255,212),  "aquamarine1" },
    { PK_XPM_RGB(118,238,198),  "aquamarine2" },
    { PK_XPM_RGB(102,205,170),  "aquamarine3" },
    { PK_XPM_RGB( 69,139,116),  "aquamarine4" },
    { PK_XPM_RGB(240,255,255),  "azure" },
    { PK_XPM_RGB(240,255,255),  "azure1" },
    { PK_XPM_RGB(224,238,238),  "azure2" },
    { PK_XPM_RGB(193,205,205),  "azure3" },
    { PK_XPM_RGB(131,139,139),  "azure4" },
    { PK_XPM_RGB(245,245,220),  "beige" },
    { PK_XPM_RGB(255,228,196),  "bisque" },
    { PK_XPM_RGB(255,228,196),  "bisque1" },
    { PK_XPM_RGB(238,213,183),  "bisque2" },
    { PK_XPM_RGB(205,183,158),  "bisque3" },
    { PK_XPM_RGB(139,125,107),  "bisque4" },
    { PK_XPM_RGB(  0,  0,  0),  "black" },
    { PK_XPM_RGB(255,235,205),  "blanchedalmond" },
    { PK_XPM_RGB(  0,  0,255),  "blue" },
    { PK_XPM_RGB(  0,  0,255),  "blue1" },
    { PK_XPM_RGB(  0,  0,238),  "blue2" },
    { PK_XPM_RGB(  0,  0,205),  "blue3" },
    { PK_XPM_RGB(  0,  0,139),  "blue4" },
    { PK_XPM_RGB(138, 43,226),  "blueviolet" },
    { PK_XPM_RGB(165, 42, 42),  "brown" },
    { PK_XPM_RGB(255, 64, 64),  "brown1" },
    { PK_XPM_RGB(238, 59, 59),  "brown2" },
    { PK_XPM_RGB(205, 51, 51),  "brown3" },
    { PK_XPM_RGB(139, 35, 35),  "brown4" },
    { PK_XPM_RGB(222,184,135),  "burlywood" },
    { PK_XPM_RGB(255,211,155),  "burlywood1" },
    { PK_XPM_RGB(238,197,145),  "burlywood2" },
    { PK_XPM_RGB(205,170,125),  "burlywood3" },
    { PK_XPM_RGB(139,115, 85),  "burlywood4" },
    { PK_XPM_RGB( 95,158,160),  "cadetblue" },
    { PK_XPM_RGB(152,245,255),  "cadetblue1" },
    { PK_XPM_RGB(142,229,238),  "cadetblue2" },
    { PK_XPM_RGB(122,197,205),  "cadetblue3" },
    { PK_XPM_RGB( 83,134,139),  "cadetblue4" },
    { PK_XPM_RGB(127,255,  0),  "chartreuse" },
    { PK_XPM_RGB(127,255,  0),  "chartreuse1" },
    { PK_XPM_RGB(118,238,  0),  "chartreuse2" },
    { PK_XPM_RGB(102,205,  0),  "chartreuse3" },
    { PK_XPM_RGB( 69,139,  0),  "chartreuse4" },
    { PK_XPM_RGB(210,105, 30),  "chocolate" },
    { PK_XPM_RGB(255,127, 36),  "chocolate1" },
    { PK_XPM_RGB(238,118, 33),  "chocolate2" },
    { PK_XPM_RGB(205,102, 29),  "chocolate3" },
    { PK_XPM_RGB(139, 69, 19),  "chocolate4" },
    { PK_XPM_RGB(255,127, 80),  "coral" },
    { PK_XPM_RGB(255,114, 86),  "coral1" },
    { PK_XPM_RGB(238,106, 80),  "coral2" },
    { PK_XPM_RGB(205, 91, 69),  "coral3" },
    { PK_XPM_RGB(139, 62, 47),  "coral4" },
    { PK_XPM_RGB(100,149,237),  "cornflowerblue" },
    { PK_XPM_RGB(255,248,220),  "cornsilk" },
    { PK_XPM_RGB(255,248,220),  "cornsilk1" },
    { PK_XPM_RGB(238,232,205),  "cornsilk2" },
    { PK_XPM_RGB(205,200,177),  "cornsilk3" },
    { PK_XPM_RGB(139,136,120),  "cornsilk4" },
    { PK_XPM_RGB(  0,255,255),  "cyan" },
    { PK_XPM_RGB(  0,255,255),  "cyan1" },
    { PK_XPM_RGB(  0,238,238),  "cyan2" },
    { PK_XPM_RGB(  0,205,205),  "cyan3" },
    { PK_XPM_RGB(  0,139,139),  "cyan4" },
    { PK_XPM_RGB(  0,  0,139),  "darkblue" },
    { PK_XPM_RGB(  0,139,139),  "darkcyan" },
    { PK_XPM_RGB(184,134, 11),  "darkgoldenrod" },
    { PK_XPM_RGB(255,185, 15),  "darkgoldenrod1" },
    { PK_XPM_RGB(238,173, 14),  "darkgoldenrod2" },
    { PK_XPM_RGB(205,149, 12),  "darkgoldenrod3" },
    { PK_XPM_RGB(139,101,  8),  "darkgoldenrod4" },
    { PK_XPM_RGB(169,169,169),  "darkgray" },
    { PK_XPM_RGB(  0,100,  0),  "darkgreen" },
    { PK_XPM_RGB(169,169,169),  "darkgrey" },
    { PK_XPM_RGB(189,183,107),  "darkkhaki" },
    { PK_XPM_RGB(139,  0,139),  "darkmagenta" },
    { PK_XPM_RGB( 85,107, 47),  "darkolivegreen" },
    { PK_XPM_RGB(202,255,112),  "darkolivegreen1" },
    { PK_XPM_RGB(188,238,104),  "darkolivegreen2" },
    { PK_XPM_RGB(162,205, 90),  "darkolivegreen3" },
    { PK_XPM_RGB(110,139, 61),  "darkolivegreen4" },
    { PK_XPM_RGB(255,140,  0),  "darkorange" },
    { PK_XPM_RGB(255,127,  0),  "darkorange1" },
    { PK_XPM_RGB(238,118,  0),  "darkorange2" },
    { PK_XPM_RGB(205,102,  0),  "darkorange3" },
    { PK_XPM_RGB(139, 69,  0),  "darkorange4" },
    { PK_XPM_RGB(153, 50,204),  "darkorchid" },
    { PK_XPM_RGB(191, 62,255),  "darkorchid1" },
    { PK_XPM_RGB(178, 58,238),  "darkorchid2" },
    { PK_XPM_RGB(154, 50,205),  "darkorchid3" },
    { PK_XPM_RGB(104, 34,139),  "darkorchid4" },
    { PK_XPM_RGB(139,  0,  0),  "darkred" },
    { PK_XPM_RGB(233,150,122),  "darksalmon" },
    { PK_XPM_RGB(143,188,143),  "darkseagreen" },
    { PK_XPM_RGB(193,255,193),  "darkseagreen1" },
    { PK_XPM_RGB(180,238,180),  "darkseagreen2" },
    { PK_XPM_RGB(155,205,155),  "darkseagreen3" },
    { PK_XPM_RGB(105,139,105),  "darkseagreen4" },
    { PK_XPM_RGB( 72, 61,139),  "darkslateblue" },
    { PK_XPM_RGB( 47, 79, 79),  "darkslategray" },
    { PK_XPM_RGB(151,255,255),  "darkslategray1" },
    { PK_XPM_RGB(141,238,238),  "darkslategray2" },
    { PK_XPM_RGB(121,205,205),  "darkslategray3" },
    { PK_XPM_RGB( 82,139,139),  "darkslategray4" },
    { PK_XPM_RGB( 47, 79, 79),  "darkslategrey" },
    { PK_XPM_RGB(  0,206,209),  "darkturquoise" },
    { PK_XPM_RGB(148,  0,211),  "darkviolet" },
    { PK_XPM_RGB(255, 20,147),  "deeppink" },
    { PK_XPM_RGB(255, 20,147),  "deeppink1" },
    { PK_XPM_RGB(238, 18,137),  "deeppink2" },
    { PK_XPM_RGB(205, 16,118),  "deeppink3" },
    { PK_XPM_RGB(139, 10, 80),  "deeppink4" },
    { PK_XPM_RGB(  0,191,255),  "deepskyblue" },
    { PK_XPM_RGB(  0,191,255),  "deepskyblue1" },
    { PK_XPM_RGB(  0,178,238),  "deepskyblue2" },
    { PK_XPM_RGB(  0,154,205),  "deepskyblue3" },
    { PK_XPM_RGB(  0,104,139),  "deepskyblue4" },
    { PK_XPM_RGB(105,105,105),  "dimgray" },
    { PK_XPM_RGB(105,105,105),  "dimgrey" },
    { PK_XPM_RGB( 30,144,255),  "dodgerblue" },
    { PK_XPM_RGB( 30,144,255),  "dodgerblue1" },
    { PK_XPM_RGB( 28,134,238),  "dodgerblue2" },
    { PK_XPM_RGB( 24,116,205),  "dodgerblue3" },
    { PK_XPM_RGB( 16, 78,139),  "dodgerblue4" },
    { PK_XPM_RGB(178, 34, 34),  "firebrick" },
    { PK_XPM_RGB(255, 48, 48),  "firebrick1" },
    { PK_XPM_RGB(238, 44, 44),  "firebrick2" },
    { PK_XPM_RGB(205, 38, 38),  "firebrick3" },
    { PK_XPM_RGB(139, 26, 26),  "firebrick4" },
    { PK_XPM_RGB(255,250,240),  "floralwhite" },
    { PK_XPM_RGB( 34,139, 34),  "forestgreen" },
    { PK_XPM_RGB(220,220,220),  "gainsboro" },
    { PK_XPM_RGB(248,248,255),  "ghostwhite" },
    { PK_XPM_RGB(255,215,  0),  "gold" },
    { PK_XPM_RGB(255,215,  0),  "gold1" },
    { PK_XPM_RGB(238,201,  0),  "gold2" },
    { PK_XPM_RGB(205,173,  0),  "gold3" },
    { PK_XPM_RGB(139,117,  0),  "gold4" },
    { PK_XPM_RGB(218,165, 32),  "goldenrod" },
    { PK_XPM_RGB(255,193, 37),  "goldenrod1" },
    { PK_XPM_RGB(238,180, 34),  "goldenrod2" },
    { PK_XPM_RGB(205,155, 29),  "goldenrod3" },
    { PK_XPM_RGB(139,105, 20),  "goldenrod4" },
    { PK_XPM_RGB(190,190,190),  "gray" },
    { PK_XPM_RGB(  0,  0,  0),  "gray0" },
    { PK_XPM_RGB(  3,  3,  3),  "gray1" },
    { PK_XPM_RGB( 26, 26, 26),  "gray10" },
    { PK_XPM_RGB(255,255,255),  "gray100" },
    { PK_XPM_RGB( 28, 28, 28),  "gray11" },
    { PK_XPM_RGB( 31, 31, 31),  "gray12" },
    { PK_XPM_RGB( 33, 33, 33),  "gray13" },
    { PK_XPM_RGB( 36, 36, 36),  "gray14" },
    { PK_XPM_RGB( 38, 38, 38),  "gray15" },
    { PK_XPM_RGB( 41, 41, 41),  "gray16" },
    { PK_XPM_RGB( 43, 43, 43),  "gray17" },
    { PK_XPM_RGB( 46, 46, 46),  "gray18" },
    { PK_XPM_RGB( 48, 48, 48),  "gray19" },
    { PK_XPM_RGB(  5,  5,  5),  "gray2" },
    { PK_XPM_RGB( 51, 51, 51),  "gray20" },
    { PK_XPM_RGB( 54, 54, 54),  "gray21" },
    { PK_XPM_RGB( 56, 56, 56),  "gray22" },
    { PK_XPM_RGB( 59, 59, 59),  "gray23" },
    { PK_XPM_RGB( 61, 61, 61),  "gray24" },
    { PK_XPM_RGB( 64, 64, 64),  "gray25" },
    { PK_XPM_RGB( 66, 66, 66),  "gray26" },
    { PK_XPM_RGB( 69, 69, 69),  "gray27" },
    { PK_XPM_RGB( 71, 71, 71),  "gray28" },
    { PK_XPM_RGB( 74, 74, 74),  "gray29" },
    { PK_XPM_RGB(  8,  8,  8),  "gray3" },
    { PK_XPM_RGB( 77, 77, 77),  "gray30" },
    { PK_XPM_RGB( 79, 79, 79),  "gray31" },
    { PK_XPM_RGB( 82, 82, 82),  "gray32" },
    { PK_XPM_RGB( 84, 84, 84),  "gray33" },
    { PK_XPM_RGB( 87, 87, 87),  "gray34" },
    { PK_XPM_RGB( 89, 89, 89),  "gray35" },
    { PK_XPM_RGB( 92, 92, 92),  "gray36" },
    { PK_XPM_RGB( 94, 94, 94),  "gray37" },
    { PK_XPM_RGB( 97, 97, 97),  "gray38" },
    { PK_XPM_RGB( 99, 99, 99),  "gray39" },
    { PK_XPM_RGB( 10, 10, 10),  "gray4" },
    { PK_XPM_RGB(102,102,102),  "gray40" },
    { PK_XPM_RGB(105,105,105),  "gray41" },
    { PK_XPM_RGB(107,107,107),  "gray42" },
    { PK_XPM_RGB(110,110,110),  "gray43" },
    { PK_XPM_RGB(112,112,112),  "gray44" },
    { PK_XPM_RGB(115,115,115),  "gray45" },
    { PK_XPM_RGB(117,117,117),  "gray46" },
    { PK_XPM_RGB(120,120,120),  "gray47" },
    { PK_XPM_RGB(122,122,122),  "gray48" },
    { PK_XPM_RGB(125,125,125),  "gray49" },
    { PK_XPM_RGB( 13, 13, 13),  "gray5" },
    { PK_XPM_RGB(127,127,127),  "gray50" },
    { PK_XPM_RGB(130,130,130),  "gray51" },
    { PK_XPM_RGB(133,133,133),  "gray52" },
    { PK_XPM_RGB(135,135,135),  "gray53" },
    { PK_XPM_RGB(138,138,138),  "gray54" },
    { PK_XPM_RGB(140,140,140),  "gray55" },
    { PK_XPM_RGB(143,143,143),  "gray56" },
    { PK_XPM_RGB(145,145,145),  "gray57" },
    { PK_XPM_RGB(148,148,148),  "gray58" },
    { PK_XPM_RGB(150,150,150),  "gray59" },
    { PK_XPM_RGB( 15, 15, 15),  "gray6" },
    { PK_XPM_RGB(153,153,153),  "gray60" },
    { PK_XPM_RGB(156,156,156),  "gray61" },
    { PK_XPM_RGB(158,158,158),  "gray62" },
    { PK_XPM_RGB(161,161,161),  "gray63" },
    { PK_XPM_RGB(163,163,163),  "gray64" },
    { PK_XPM_RGB(166,166,166),  "gray65" },
    { PK_XPM_RGB(168,168,168),  "gray66" },
    { PK_XPM_RGB(171,171,171),  "gray67" },
    { PK_XPM_RGB(173,173,173),  "gray68" },
    { PK_XPM_RGB(176,176,176),  "gray69" },
    { PK_XPM_RGB( 18, 18, 18),  "gray7" },
    { PK_XPM_RGB(179,179,179),  "gray70" },
    { PK_XPM_RGB(181,181,181),  "gray71" },
    { PK_XPM_RGB(184,184,184),  "gray72" },
    { PK_XPM_RGB(186,186,186),  "gray73" },
    { PK_XPM_RGB(189,189,189),  "gray74" },
    { PK_XPM_RGB(191,191,191),  "gray75" },
    { PK_XPM_RGB(194,194,194),  "gray76" },
    { PK_XPM_RGB(196,196,196),  "gray77" },
    { PK_XPM_RGB(199,199,199),  "gray78" },
    { PK_XPM_RGB(201,201,201),  "gray79" },
    { PK_XPM_RGB( 20, 20, 20),  "gray8" },
    { PK_XPM_RGB(204,204,204),  "gray80" },
    { PK_XPM_RGB(207,207,207),  "gray81" },
    { PK_XPM_RGB(209,209,209),  "gray82" },
    { PK_XPM_RGB(212,212,212),  "gray83" },
    { PK_XPM_RGB(214,214,214),  "gray84" },
    { PK_XPM_RGB(217,217,217),  "gray85" },
    { PK_XPM_RGB(219,219,219),  "gray86" },
    { PK_XPM_RGB(222,222,222),  "gray87" },
    { PK_XPM_RGB(224,224,224),  "gray88" },
    { PK_XPM_RGB(227,227,227),  "gray89" },
    { PK_XPM_RGB( 23, 23, 23),  "gray9" },
    { PK_XPM_RGB(229,229,229),  "gray90" },
    { PK_XPM_RGB(232,232,232),  "gray91" },
    { PK_XPM_RGB(235,235,235),  "gray92" },
    { PK_XPM_RGB(237,237,237),  "gray93" },
    { PK_XPM_RGB(240,240,240),  "gray94" },
    { PK_XPM_RGB(242,242,242),  "gray95" },
    { PK_XPM_RGB(245,245,245),  "gray96" },
    { PK_XPM_RGB(247,247,247),  "gray97" },
    { PK_XPM_RGB(250,250,250),  "gray98" },
    { PK_XPM_RGB(252,252,252),  "gray99" },
    { PK_XPM_RGB(  0,255,  0),  "green" },
    { PK_XPM_RGB(  0,255,  0),  "green1" },
    { PK_XPM_RGB(  0,238,  0),  "green2" },
    { PK_XPM_RGB(  0,205,  0),  "green3" },
    { PK_XPM_RGB(  0,139,  0),  "green4" },
    { PK_XPM_RGB(173,255, 47),  "greenyellow" },
    { PK_XPM_RGB(190,190,190),  "grey" },
    { PK_XPM_RGB(  0,  0,  0),  "grey0" },
    { PK_XPM_RGB(  3,  3,  3),  "grey1" },
    { PK_XPM_RGB( 26, 26, 26),  "grey10" },
    { PK_XPM_RGB(255,255,255),  "grey100" },
    { PK_XPM_RGB( 28, 28, 28),  "grey11" },
    { PK_XPM_RGB( 31, 31, 31),  "grey12" },
    { PK_XPM_RGB( 33, 33, 33),  "grey13" },
    { PK_XPM_RGB( 36, 36, 36),  "grey14" },
    { PK_XPM_RGB( 38, 38, 38),  "grey15" },
    { PK_XPM_RGB( 41, 41, 41),  "grey16" },
    { PK_XPM_RGB( 43, 43, 43),  "grey17" },
    { PK_XPM_RGB( 46, 46, 46),  "grey18" },
    { PK_XPM_RGB( 48, 48, 48),  "grey19" },
    { PK_XPM_RGB(  5,  5,  5),  "grey2" },
    { PK_XPM_RGB( 51, 51, 51),  "grey20" },
    { PK_XPM_RGB( 54, 54, 54),  "grey21" },
    { PK_XPM_RGB( 56, 56, 56),  "grey22" },
    { PK_XPM_RGB( 59, 59, 59),  "grey23" },
    { PK_XPM_RGB( 61, 61, 61),  "grey24" },
    { PK_XPM_RGB( 64, 64, 64),  "grey25" },
    { PK_XPM_RGB( 66, 66, 66),  "grey26" },
    { PK_XPM_RGB( 69, 69, 69),  "grey27" },
    { PK_XPM_RGB( 71, 71, 71),  "grey28" },
    { PK_XPM_RGB( 74, 74, 74),  "grey29" },
    { PK_XPM_RGB(  8,  8,  8),  "grey3" },
    { PK_XPM_RGB( 77, 77, 77),  "grey30" },
    { PK_XPM_RGB( 79, 79, 79),  "grey31" },
    { PK_XPM_RGB( 82, 82, 82),  "grey32" },
    { PK_XPM_RGB( 84, 84, 84),  "grey33" },
    { PK_XPM_RGB( 87, 87, 87),  "grey34" },
    { PK_XPM_RGB( 89, 89, 89),  "grey35" },
    { PK_XPM_RGB( 92, 92, 92),  "grey36" },
    { PK_XPM_RGB( 94, 94, 94),  "grey37" },
    { PK_XPM_RGB( 97, 97, 97),  "grey38" },
    { PK_XPM_RGB( 99, 99, 99),  "grey39" },
    { PK_XPM_RGB( 10, 10, 10),  "grey4" },
    { PK_XPM_RGB(102,102,102),  "grey40" },
    { PK_XPM_RGB(105,105,105),  "grey41" },
    { PK_XPM_RGB(107,107,107),  "grey42" },
    { PK_XPM_RGB(110,110,110),  "grey43" },
    { PK_XPM_RGB(112,112,112),  "grey44" },
    { PK_XPM_RGB(115,115,115),  "grey45" },
    { PK_XPM_RGB(117,117,117),  "grey46" },
    { PK_XPM_RGB(120,120,120),  "grey47" },
    { PK_XPM_RGB(122,122,122),  "grey48" },
    { PK_XPM_RGB(125,125,125),  "grey49" },
    { PK_XPM_RGB( 13, 13, 13),  "grey5" },
    { PK_XPM_RGB(127,127,127),  "grey50" },
    { PK_XPM_RGB(130,130,130),  "grey51" },
    { PK_XPM_RGB(133,133,133),  "grey52" },
    { PK_XPM_RGB(135,135,135),  "grey53" },
    { PK_XPM_RGB(138,138,138),  "grey54" },
    { PK_XPM_RGB(140,140,140),  "grey55" },
    { PK_XPM_RGB(143,143,143),  "grey56" },
    { PK_XPM_RGB(145,145,145),  "grey57" },
    { PK_XPM_RGB(148,148,148),  "grey58" },
    { PK_XPM_RGB(150,150,150),  "grey59" },
    { PK_XPM_RGB( 15, 15, 15),  "grey6" },
    { PK_XPM_RGB(153,153,153),  "grey60" },
    { PK_XPM_RGB(156,156,156),  "grey61" },
    { PK_XPM_RGB(158,158,158),  "grey62" },
    { PK_XPM_RGB(161,161,161),  "grey63" },
    { PK_XPM_RGB(163,163,163),  "grey64" },
    { PK_XPM_RGB(166,166,166),  "grey65" },
    { PK_XPM_RGB(168,168,168),  "grey66" },
    { PK_XPM_RGB(171,171,171),  "grey67" },
    { PK_XPM_RGB(173,173,173),  "grey68" },
    { PK_XPM_RGB(176,176,176),  "grey69" },
    { PK_XPM_RGB( 18, 18, 18),  "grey7" },
    { PK_XPM_RGB(179,179,179),  "grey70" },
    { PK_XPM_RGB(181,181,181),  "grey71" },
    { PK_XPM_RGB(184,184,184),  "grey72" },
    { PK_XPM_RGB(186,186,186),  "grey73" },
    { PK_XPM_RGB(189,189,189),  "grey74" },
    { PK_XPM_RGB(191,191,191),  "grey75" },
    { PK_XPM_RGB(194,194,194),  "grey76" },
    { PK_XPM_RGB(196,196,196),  "grey77" },
    { PK_XPM_RGB(199,199,199),  "grey78" },
    { PK_XPM_RGB(201,201,201),  "grey79" },
    { PK_XPM_RGB( 20, 20, 20),  "grey8" },
    { PK_XPM_RGB(204,204,204),  "grey80" },
    { PK_XPM_RGB(207,207,207),  "grey81" },
    { PK_XPM_RGB(209,209,209),  "grey82" },
    { PK_XPM_RGB(212,212,212),  "grey83" },
    { PK_XPM_RGB(214,214,214),  "grey84" },
    { PK_XPM_RGB(217,217,217),  "grey85" },
    { PK_XPM_RGB(219,219,219),  "grey86" },
    { PK_XPM_RGB(222,222,222),  "grey87" },
    { PK_XPM_RGB(224,224,224),  "grey88" },
    { PK_XPM_RGB(227,227,227),  "grey89" },
    { PK_XPM_RGB( 23, 23, 23),  "grey9" },
    { PK_XPM_RGB(229,229,229),  "grey90" },
    { PK_XPM_RGB(232,232,232),  "grey91" },
    { PK_XPM_RGB(235,235,235),  "grey92" },
    { PK_XPM_RGB(237,237,237),  "grey93" },
    { PK_XPM_RGB(240,240,240),  "grey94" },
    { PK_XPM_RGB(242,242,242),  "grey95" },
    { PK_XPM_RGB(245,245,245),  "grey96" },
    { PK_XPM_RGB(247,247,247),  "grey97" },
    { PK_XPM_RGB(250,250,250),  "grey98" },
    { PK_XPM_RGB(252,252,252),  "grey99" },
    { PK_XPM_RGB(240,255,240),  "honeydew" },
    { PK_XPM_RGB(240,255,240),  "honeydew1" },
    { PK_XPM_RGB(224,238,224),  "honeydew2" },
    { PK_XPM_RGB(193,205,193),  "honeydew3" },
    { PK_XPM_RGB(131,139,131),  "honeydew4" },
    { PK_XPM_RGB(255,105,180),  "hotpink" },
    { PK_XPM_RGB(255,110,180),  "hotpink1" },
    { PK_XPM_RGB(238,106,167),  "hotpink2" },
    { PK_XPM_RGB(205, 96,144),  "hotpink3" },
    { PK_XPM_RGB(139, 58, 98),  "hotpink4" },
    { PK_XPM_RGB(205, 92, 92),  "indianred" },
    { PK_XPM_RGB(255,106,106),  "indianred1" },
    { PK_XPM_RGB(238, 99, 99),  "indianred2" },
    { PK_XPM_RGB(205, 85, 85),  "indianred3" },
    { PK_XPM_RGB(139, 58, 58),  "indianred4" },
    { PK_XPM_RGB(255,255,240),  "ivory" },
    { PK_XPM_RGB(255,255,240),  "ivory1" },
    { PK_XPM_RGB(238,238,224),  "ivory2" },
    { PK_XPM_RGB(205,205,193),  "ivory3" },
    { PK_XPM_RGB(139,139,131),  "ivory4" },
    { PK_XPM_RGB(240,230,140),  "khaki" },
    { PK_XPM_RGB(255,246,143),  "khaki1" },
    { PK_XPM_RGB(238,230,133),  "khaki2" },
    { PK_XPM_RGB(205,198,115),  "khaki3" },
    { PK_XPM_RGB(139,134, 78),  "khaki4" },
    { PK_XPM_RGB(230,230,250),  "lavender" },
    { PK_XPM_RGB(255,240,245),  "lavenderblush" },
    { PK_XPM_RGB(255,240,245),  "lavenderblush1" },
    { PK_XPM_RGB(238,224,229),  "lavenderblush2" },
    { PK_XPM_RGB(205,193,197),  "lavenderblush3" },
    { PK_XPM_RGB(139,131,134),  "lavenderblush4" },
    { PK_XPM_RGB(124,252,  0),  "lawngreen" },
    { PK_XPM_RGB(255,250,205),  "lemonchiffon" },
    { PK_XPM_RGB(255,250,205),  "lemonchiffon1" },
    { PK_XPM_RGB(238,233,191),  "lemonchiffon2" },
    { PK_XPM_RGB(205,201,165),  "lemonchiffon3" },
    { PK_XPM_RGB(139,137,112),  "lemonchiffon4" },
    { PK_XPM_RGB(173,216,230),  "lightblue" },
    { PK_XPM_RGB(191,239,255),  "lightblue1" },
    { PK_XPM_RGB(178,223,238),  "lightblue2" },
    { PK_XPM_RGB(154,192,205),  "lightblue3" },
    { PK_XPM_RGB(104,131,139),  "lightblue4" },
    { PK_XPM_RGB(240,128,128),  "lightcoral" },
    { PK_XPM_RGB(224,255,255),  "lightcyan" },
    { PK_XPM_RGB(224,255,255),  "lightcyan1" },
    { PK_XPM_RGB(209,238,238),  "lightcyan2" },
    { PK_XPM_RGB(180,205,205),  "lightcyan3" },
    { PK_XPM_RGB(122,139,139),  "lightcyan4" },
    { PK_XPM_RGB(238,221,130),  "lightgoldenrod" },
    { PK_XPM_RGB(255,236,139),  "lightgoldenrod1" },
    { PK_XPM_RGB(238,220,130),  "lightgoldenrod2" },
    { PK_XPM_RGB(205,190,112),  "lightgoldenrod3" },
    { PK_XPM_RGB(139,129, 76),  "lightgoldenrod4" },
    { PK_XPM_RGB(250,250,210),  "lightgoldenrodyellow" },
    { PK_XPM_RGB(211,211,211),  "lightgray" },
    { PK_XPM_RGB(144,238,144),  "lightgreen" },
    { PK_XPM_RGB(211,211,211),  "lightgrey" },
    { PK_XPM_RGB(255,182,193),  "lightpink" },
    { PK_XPM_RGB(255,174,185),  "lightpink1" },
    { PK_XPM_RGB(238,162,173),  "lightpink2" },
    { PK_XPM_RGB(205,140,149),  "lightpink3" },
    { PK_XPM_RGB(139, 95,101),  "lightpink4" },
    { PK_XPM_RGB(255,160,122),  "lightsalmon" },
    { PK_XPM_RGB(255,160,122),  "lightsalmon1" },
    { PK_XPM_RGB(238,149,114),  "lightsalmon2" },
    { PK_XPM_RGB(205,129, 98),  "lightsalmon3" },
    { PK_XPM_RGB(139, 87, 66),  "lightsalmon4" },
    { PK_XPM_RGB( 32,178,170),  "lightseagreen" },
    { PK_XPM_RGB(135,206,250),  "lightskyblue" },
    { PK_XPM_RGB(176,226,255),  "lightskyblue1" },
    { PK_XPM_RGB(164,211,238),  "lightskyblue2" },
    { PK_XPM_RGB(141,182,205),  "lightskyblue3" },
    { PK_XPM_RGB( 96,123,139),  "lightskyblue4" },
    { PK_XPM_RGB(132,112,255),  "lightslateblue" },
    { PK_XPM_RGB(119,136,153),  "lightslategray" },
    { PK_XPM_RGB(119,136,153),  "lightslategrey" },
    { PK_XPM_RGB(176,196,222),  "lightsteelblue" },
    { PK_XPM_RGB(202,225,255),  "lightsteelblue1" },
    { PK_XPM_RGB(188,210,238),  "lightsteelblue2" },
    { PK_XPM_RGB(162,181,205),  "lightsteelblue3" },
    { PK_XPM_RGB(110,123,139),  "lightsteelblue4" },
    { PK_XPM_RGB(255,255,224),  "lightyellow" },
    { PK_XPM_RGB(255,255,224),  "lightyellow1" },
    { PK_XPM_RGB(238,238,209),  "lightyellow2" },
    { PK_XPM_RGB(205,205,180),  "lightyellow3" },
    { PK_XPM_RGB(139,139,122),  "lightyellow4" },
    { PK_XPM_RGB( 50,205, 50),  "limegreen" },
    { PK_XPM_RGB(250,240,230),  "linen" },
    { PK_XPM_RGB(255,  0,255),  "magenta" },
    { PK_XPM_RGB(255,  0,255),  "magenta1" },
    { PK_XPM_RGB(238,  0,238),  "magenta2" },
    { PK_XPM_RGB(205,  0,205),  "magenta3" },
    { PK_XPM_RGB(139,  0,139),  "magenta4" },
    { PK_XPM_RGB(176, 48, 96),  "maroon" },
    { PK_XPM_RGB(255, 52,179),  "maroon1" },
    { PK_XPM_RGB(238, 48,167),  "maroon2" },
    { PK_XPM_RGB(205, 41,144),  "maroon3" },
    { PK_XPM_RGB(139, 28, 98),  "maroon4" },
    { PK_XPM_RGB(102,205,170),  "mediumaquamarine" },
    { PK_XPM_RGB(  0,  0,205),  "mediumblue" },
    { PK_XPM_RGB(186, 85,211),  "mediumorchid" },
    { PK_XPM_RGB(224,102,255),  "mediumorchid1" },
    { PK_XPM_RGB(209, 95,238),  "mediumorchid2" },
    { PK_XPM_RGB(180, 82,205),  "mediumorchid3" },
    { PK_XPM_RGB(122, 55,139),  "mediumorchid4" },
    { PK_XPM_RGB(147,112,219),  "mediumpurple" },
    { PK_XPM_RGB(171,130,255),  "mediumpurple1" },
    { PK_XPM_RGB(159,121,238),  "mediumpurple2" },
    { PK_XPM_RGB(137,104,205),  "mediumpurple3" },
    { PK_XPM_RGB( 93, 71,139),  "mediumpurple4" },
    { PK_XPM_RGB( 60,179,113),  "mediumseagreen" },
    { PK_XPM_RGB(123,104,238),  "mediumslateblue" },
    { PK_XPM_RGB(  0,250,154),  "mediumspringgreen" },
    { PK_XPM_RGB( 72,209,204),  "mediumturquoise" },
    { PK_XPM_RGB(199, 21,133),  "mediumvioletred" },
    { PK_XPM_RGB( 25, 25,112),  "midnightblue" },
    { PK_XPM_RGB(245,255,250),  "mintcream" },
    { PK_XPM_RGB(255,228,225),  "mistyrose" },
    { PK_XPM_RGB(255,228,225),  "mistyrose1" },
    { PK_XPM_RGB(238,213,210),  "mistyrose2" },
    { PK_XPM_RGB(205,183,181),  "mistyrose3" },
    { PK_XPM_RGB(139,125,123),  "mistyrose4" },
    { PK_XPM_RGB(255,228,181),  "moccasin" },
    { PK_XPM_RGB(255,222,173),  "navajowhite" },
    { PK_XPM_RGB(255,222,173),  "navajowhite1" },
    { PK_XPM_RGB(238,207,161),  "navajowhite2" },
    { PK_XPM_RGB(205,179,139),  "navajowhite3" },
    { PK_XPM_RGB(139,121, 94),  "navajowhite4" },
    { PK_XPM_RGB(  0,  0,128),  "navy" },
    { PK_XPM_RGB(  0,  0,128),  "navyblue" },
    { PK_XPM_RGB(253,245,230),  "oldlace" },
    { PK_XPM_RGB(107,142, 35),  "olivedrab" },
    { PK_XPM_RGB(192,255, 62),  "olivedrab1" },
    { PK_XPM_RGB(179,238, 58),  "olivedrab2" },
    { PK_XPM_RGB(154,205, 50),  "olivedrab3" },
    { PK_XPM_RGB(105,139, 34),  "olivedrab4" },
    { PK_XPM_RGB(255,165,  0),  "orange" },
    { PK_XPM_RGB(255,165,  0),  "orange1" },
    { PK_XPM_RGB(238,154,  0),  "orange2" },
    { PK_XPM_RGB(205,133,  0),  "orange3" },
    { PK_XPM_RGB(139, 90,  0),  "orange4" },
    { PK_XPM_RGB(255, 69,  0),  "orangered" },
    { PK_XPM_RGB(255, 69,  0),  "orangered1" },
    { PK_XPM_RGB(238, 64,  0),  "orangered2" },
    { PK_XPM_RGB(205, 55,  0),  "orangered3" },
    { PK_XPM_RGB(139, 37,  0),  "orangered4" },
    { PK_XPM_RGB(218,112,214),  "orchid" },
    { PK_XPM_RGB(255,131,250),  "orchid1" },
    { PK_XPM_RGB(238,122,233),  "orchid2" },
    { PK_XPM_RGB(205,105,201),  "orchid3" },
    { PK_XPM_RGB(139, 71,137),  "orchid4" },
    { PK_XPM_RGB(238,232,170),  "palegoldenrod" },
    { PK_XPM_RGB(152,251,152),  "palegreen" },
    { PK_XPM_RGB(154,255,154),  "palegreen1" },
    { PK_XPM_RGB(144,238,144),  "palegreen2" },
    { PK_XPM_RGB(124,205,124),  "palegreen3" },
    { PK_XPM_RGB( 84,139, 84),  "palegreen4" },
    { PK_XPM_RGB(175,238,238),  "paleturquoise" },
    { PK_XPM_RGB(187,255,255),  "paleturquoise1" },
    { PK_XPM_RGB(174,238,238),  "paleturquoise2" },
    { PK_XPM_RGB(150,205,205),  "paleturquoise3" },
    { PK_XPM_RGB(102,139,139),  "paleturquoise4" },
    { PK_XPM_RGB(219,112,147),  "palevioletred" },
    { PK_XPM_RGB(255,130,171),  "palevioletred1" },
    { PK_XPM_RGB(238,121,159),  "palevioletred2" },
    { PK_XPM_RGB(205,104,137),  "palevioletred3" },
    { PK_XPM_RGB(139, 71, 93),  "palevioletred4" },
    { PK_XPM_RGB(255,239,213),  "papayawhip" },
    { PK_XPM_RGB(255,218,185),  "peachpuff" },
    { PK_XPM_RGB(255,218,185),  "peachpuff1" },
    { PK_XPM_RGB(238,203,173),  "peachpuff2" },
    { PK_XPM_RGB(205,175,149),  "peachpuff3" },
    { PK_XPM_RGB(139,119,101),  "peachpuff4" },
    { PK_XPM_RGB(205,133, 63),  "peru" },
    { PK_XPM_RGB(255,192,203),  "pink" },
    { PK_XPM_RGB(255,181,197),  "pink1" },
    { PK_XPM_RGB(238,169,184),  "pink2" },
    { PK_XPM_RGB(205,145,158),  "pink3" },
    { PK_XPM_RGB(139, 99,108),  "pink4" },
    { PK_XPM_RGB(221,160,221),  "plum" },
    { PK_XPM_RGB(255,187,255),  "plum1" },
    { PK_XPM_RGB(238,174,238),  "plum2" },
    { PK_XPM_RGB(205,150,205),  "plum3" },
    { PK_XPM_RGB(139,102,139),  "plum4" },
    { PK_XPM_RGB(176,224,230),  "powderblue" },
    { PK_XPM_RGB(160, 32,240),  "purple" },
    { PK_XPM_RGB(155, 48,255),  "purple1" },
    { PK_XPM_RGB(145, 44,238),  "purple2" },
    { PK_XPM_RGB(125, 38,205),  "purple3" },
    { PK_XPM_RGB( 85, 26,139),  "purple4" },
    { PK_XPM_RGB(255,  0,  0),  "red" },
    { PK_XPM_RGB(255,  0,  0),  "red1" },
    { PK_XPM_RGB(238,  0,  0),  "red2" },
    { PK_XPM_RGB(205,  0,  0),  "red3" },
    { PK_XPM_RGB(139,  0,  0),  "red4" },
    { PK_XPM_RGB(188,143,143),  "rosybrown" },
    { PK_XPM_RGB(255,193,193),  "rosybrown1" },
    { PK_XPM_RGB(238,180,180),  "rosybrown2" },
    { PK_XPM_RGB(205,155,155),  "rosybrown3" },
    { PK_XPM_RGB(139,105,105),  "rosybrown4" },
    { PK_XPM_RGB( 65,105,225),  "royalblue" },
    { PK_XPM_RGB( 72,118,255),  "royalblue1" },
    { PK_XPM_RGB( 67,110,238),  "royalblue2" },
    { PK_XPM_RGB( 58, 95,205),  "royalblue3" },
    { PK_XPM_RGB( 39, 64,139),  "royalblue4" },
    { PK_XPM_RGB(139, 69, 19),  "saddlebrown" },
    { PK_XPM_RGB(250,128,114),  "salmon" },
    { PK_XPM_RGB(255,140,105),  "salmon1" },
    { PK_XPM_RGB(238,130, 98),  "salmon2" },
    { PK_XPM_RGB(205,112, 84),  "salmon3" },
    { PK_XPM_RGB(139, 76, 57),  "salmon4" },
    { PK_XPM_RGB(244,164, 96),  "sandybrown" },
    { PK_XPM_RGB( 46,139, 87),  "seagreen" },
    { PK_XPM_RGB( 84,255,159),  "seagreen1" },
    { PK_XPM_RGB( 78,238,148),  "seagreen2" },
    { PK_XPM_RGB( 67,205,128),  "seagreen3" },
    { PK_XPM_RGB( 46,139, 87),  "seagreen4" },
    { PK_XPM_RGB(255,245,238),  "seashell" },
    { PK_XPM_RGB(255,245,238),  "seashell1" },
    { PK_XPM_RGB(238,229,222),  "seashell2" },
    { PK_XPM_RGB(205,197,191),  "seashell3" },
    { PK_XPM_RGB(139,134,130),  "seashell4" },
    { PK_XPM_RGB(160, 82, 45),  "sienna" },
    { PK_XPM_RGB(255,130, 71),  "sienna1" },
    { PK_XPM_RGB(238,121, 66),  "sienna2" },
    { PK_XPM_RGB(205,104, 57),  "sienna3" },
    { PK_XPM_RGB(139, 71, 38),  "sienna4" },
    { PK_XPM_RGB(135,206,235),  "skyblue" },
    { PK_XPM_RGB(135,206,255),  "skyblue1" },
    { PK_XPM_RGB(126,192,238),  "skyblue2" },
    { PK_XPM_RGB(108,166,205),  "skyblue3" },
    { PK_XPM_RGB( 74,112,139),  "skyblue4" },
    { PK_XPM_RGB(106, 90,205),  "slateblue" },
    { PK_XPM_RGB(131,111,255),  "slateblue1" },
    { PK_XPM_RGB(122,103,238),  "slateblue2" },
    { PK_XPM_RGB(105, 89,205),  "slateblue3" },
    { PK_XPM_RGB( 71, 60,139),  "slateblue4" },
    { PK_XPM_RGB(112,128,144),  "slategray" },
    { PK_XPM_RGB(198,226,255),  "slategray1" },
    { PK_XPM_RGB(185,211,238),  "slategray2" },
    { PK_XPM_RGB(159,182,205),  "slategray3" },
    { PK_XPM_RGB(108,123,139),  "slategray4" },
    { PK_XPM_RGB(112,128,144),  "slategrey" },
    { PK_XPM_RGB(255,250,250),  "snow" },
    { PK_XPM_RGB(255,250,250),  "snow1" },
    { PK_XPM_RGB(238,233,233),  "snow2" },
    { PK_XPM_RGB(205,201,201),  "snow3" },
    { PK_XPM_RGB(139,137,137),  "snow4" },
    { PK_XPM_RGB(  0,255,127),  "springgreen" },
    { PK_XPM_RGB(  0,255,127),  "springgreen1" },
    { PK_XPM_RGB(  0,238,118),  "springgreen2" },
    { PK_XPM_RGB(  0,205,102),  "springgreen3" },
    { PK_XPM_RGB(  0,139, 69),  "springgreen4" },
    { PK_XPM_RGB( 70,130,180),  "steelblue" },
    { PK_XPM_RGB( 99,184,255),  "steelblue1" },
    { PK_XPM_RGB( 92,172,238),  "steelblue2" },
    { PK_XPM_RGB( 79,148,205),  "steelblue3" },
    { PK_XPM_RGB( 54,100,139),  "steelblue4" },
    { PK_XPM_RGB(210,180,140),  "tan" },
    { PK_XPM_RGB(255,165, 79),  "tan1" },
    { PK_XPM_RGB(238,154, 73),  "tan2" },
    { PK_XPM_RGB(205,133, 63),  "tan3" },
    { PK_XPM_RGB(139, 90, 43),  "tan4" },
    { PK_XPM_RGB(216,191,216),  "thistle" },
    { PK_XPM_RGB(255,225,255),  "thistle1" },
    { PK_XPM_RGB(238,210,238),  "thistle2" },
    { PK_XPM_RGB(205,181,205),  "thistle3" },
    { PK_XPM_RGB(139,123,139),  "thistle4" },
    { PK_XPM_RGB(255, 99, 71),  "tomato" },
    { PK_XPM_RGB(255, 99, 71),  "tomato1" },
    { PK_XPM_RGB(238, 92, 66),  "tomato2" },
    { PK_XPM_RGB(205, 79, 57),  "tomato3" },
    { PK_XPM_RGB(139, 54, 38),  "tomato4" },
    { PK_XPM_RGB( 64,224,208),  "turquoise" },
    { PK_XPM_RGB(  0,245,255),  "turquoise1" },
    { PK_XPM_RGB(  0,229,238),  "turquoise2" },
    { PK_XPM_RGB(  0,197,205),  "turquoise3" },
    { PK_XPM_RGB(  0,134,139),  "turquoise4" },
    { PK_XPM_RGB(238,130,238),  "violet" },
    { PK_XPM_RGB(208, 32,144),  "violetred" },
    { PK_XPM_RGB(255, 62,150),  "violetred1" },
    { PK_XPM_RGB(238, 58,140),  "violetred2" },
    { PK_XPM_RGB(205, 50,120),  "violetred3" },
    { PK_XPM_RGB(139, 34, 82),  "violetred4" },
    { PK_XPM_RGB(245,222,179),  "wheat" },
    { PK_XPM_RGB(255,231,186),  "wheat1" },
    { PK_XPM_RGB(238,216,174),  "wheat2" },
    { PK_XPM_RGB(205,186,150),  "wheat3" },
    { PK_XPM_RGB(139,126,102),  "wheat4" },
    { PK_XPM_RGB(255,255,255),  "white" },
    { PK_XPM_RGB(245,245,245),  "whitesmoke" },
    { PK_XPM_RGB(255,255,  0),  "yellow" },
    { PK_XPM_RGB(255,255,  0),  "yellow1" },
    { PK_XPM_RGB(238,238,  0),  "yellow2" },
    { PK_XPM_RGB(205,205,  0),  "yellow3" },
    { PK_XPM_RGB(139,139,  0),  "yellow4" },
    { PK_XPM_RGB(154,205, 50),  "yellowgreen" }
};
#undef PK_XPM_RGB


std::vector<std::string> quotedStrings(const std::string &text)
{
    std::vector<std::string> result;
    for (std::size_t position = 0; position < text.size();) {
        position = text.find('"', position);
        if (position == std::string::npos) break;
        ++position;
        std::string value;
        bool closed = false;
        while (position < text.size()) {
            const char character = text[position++];
            if (character == '"') {
                closed = true;
                break;
            }
            if (character != '\\') {
                value.push_back(character);
                continue;
            }
            if (position >= text.size()) return {};
            char escaped = text[position++];
            if (escaped >= '0' && escaped <= '7') {
                unsigned octal = static_cast<unsigned>(escaped - '0');
                for (int digit = 1; digit < 3 && position < text.size() &&
                     text[position] >= '0' && text[position] <= '7'; ++digit) {
                    octal = octal * 8u + static_cast<unsigned>(text[position++] - '0');
                }
                value.push_back(static_cast<char>(octal));
            } else {
                value.push_back(escaped == 'n' ? '\n' : escaped == 't' ? '\t' : escaped);
            }
        }
        if (!closed) return {};
        result.push_back(std::move(value));
    }
    return result;
}

bool hex(char character, unsigned &value)
{
    if (character >= '0' && character <= '9') value = static_cast<unsigned>(character - '0');
    else if (character >= 'a' && character <= 'f') value = static_cast<unsigned>(character - 'a' + 10);
    else if (character >= 'A' && character <= 'F') value = static_cast<unsigned>(character - 'A' + 10);
    else return false;
    return true;
}

bool parseHexComponent(const std::string &text, std::size_t begin, std::size_t digits, uint8_t &value)
{
    unsigned component = 0;
    for (std::size_t i = 0; i < digits; ++i) {
        unsigned digit = 0;
        if (!hex(text[begin + i], digit)) return false;
        component = component * 16u + digit;
    }
    const unsigned maximum = (1u << (4u * static_cast<unsigned>(digits))) - 1u;
    value = static_cast<uint8_t>((component * 255u + maximum / 2u) / maximum);
    return true;
}

bool color(const std::string &source, uint32_t &argb)
{
    std::string value = source;
    value.erase(value.begin(), std::find_if(value.begin(), value.end(), [](unsigned char c) {
        return !std::isspace(c);
    }));
    while (!value.empty() && std::isspace(static_cast<unsigned char>(value.back()))) value.pop_back();
    std::string lower = value;
    std::transform(lower.begin(), lower.end(), lower.begin(), [](unsigned char c) {
        return static_cast<char>(std::tolower(c));
    });
    if (lower == "none") {
        argb = 0;
        return true;
    }
    const auto namedColor = std::lower_bound(
        std::begin(kXpmRgbTable), std::end(kXpmRgbTable), lower,
        [](const XpmRgbData &entry, const std::string &name) {
            return entry.name < name;
        });
    if (namedColor != std::end(kXpmRgbTable) && lower == namedColor->name) {
        argb = namedColor->value;
        return true;
    }
    if (value.empty() || value[0] != '#' ||
        (value.size() != 4 && value.size() != 7 && value.size() != 13)) return false;
    const std::size_t digits = (value.size() - 1) / 3;
    uint8_t red = 0, green = 0, blue = 0;
    if (!parseHexComponent(value, 1, digits, red) ||
        !parseHexComponent(value, 1 + digits, digits, green) ||
        !parseHexComponent(value, 1 + 2 * digits, digits, blue)) return false;
    argb = 0xFF000000u | (static_cast<uint32_t>(red) << 16) |
           (static_cast<uint32_t>(green) << 8) | blue;
    return true;
}

bool isXpm(const uint8_t *data, std::size_t size)
{
    if (!data || size < 8) return false;
    const std::string prefix(reinterpret_cast<const char *>(data), std::min<std::size_t>(size, 256));
    return prefix.find("XPM") != std::string::npos;
}

PkImage decodeXpm(const uint8_t *data, std::size_t size)
{
    if (!isXpm(data, size)) return PkImage();
    const std::string text(reinterpret_cast<const char *>(data), size);
    std::vector<std::string> lines = quotedStrings(text);
    if (lines.empty()) return PkImage();

    uint64_t width = 0, height = 0, count = 0, charsPerPixel = 0;
    std::istringstream header(lines[0]);
    if (!(header >> width >> height >> count >> charsPerPixel) ||
        width == 0 || height == 0 || count == 0 || charsPerPixel == 0 ||
        width > 32767 || height > 32767 || count > 65536 || charsPerPixel > 8 ||
        width * height > (512u * 1024u * 1024u) / 4u ||
        lines.size() < 1u + count + height) return PkImage();

    std::unordered_map<std::string, uint32_t> colors;
    colors.reserve(static_cast<std::size_t>(count));
    for (uint64_t index = 0; index < count; ++index) {
        const std::string &line = lines[1u + static_cast<std::size_t>(index)];
        if (line.size() < charsPerPixel) return PkImage();
        const std::string key = line.substr(0, static_cast<std::size_t>(charsPerPixel));
        const std::string attributes = line.substr(static_cast<std::size_t>(charsPerPixel));
        std::istringstream stream(attributes);
        std::vector<std::string> tokens;
        for (std::string token; stream >> token;) tokens.push_back(std::move(token));
        auto isField = [](const std::string &token) {
            return token == "c" || token == "g" || token == "g4" || token == "m" || token == "s";
        };
        std::size_t fieldIndex = tokens.size();
        for (const char *candidate : {"c", "g", "g4", "m"}) {
            const auto found = std::find(tokens.begin(), tokens.end(), candidate);
            if (found != tokens.end()) { fieldIndex = static_cast<std::size_t>(found - tokens.begin()); break; }
        }
        std::string selected;
        for (std::size_t token = fieldIndex + 1; token < tokens.size() && !isField(tokens[token]); ++token) {
            selected += tokens[token];
        }
        uint32_t argb = 0;
        if (selected.empty() || !color(selected, argb)) return PkImage();
        colors.emplace(key, argb);
    }

    PkImage image(static_cast<int>(width), static_cast<int>(height), PkImage::Format_ARGB32);
    if (image.isNull()) return PkImage();
    for (uint64_t y = 0; y < height; ++y) {
        const std::string &row = lines[1u + static_cast<std::size_t>(count + y)];
        if (row.size() < width * charsPerPixel) return PkImage();
        for (uint64_t x = 0; x < width; ++x) {
            const auto found = colors.find(row.substr(static_cast<std::size_t>(x * charsPerPixel),
                                                       static_cast<std::size_t>(charsPerPixel)));
            if (found == colors.end()) return PkImage();
            image.setPixel(static_cast<int>(x), static_cast<int>(y), found->second);
        }
    }
    return image;
}

} // namespace

PkImageFileDecoderHandler pkXpmImageCodecHandler()
{
    return {
        "qt.xpm", 900, {"xpm"},
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return isXpm(data, size);
        },
        [](const uint8_t *data, std::size_t size, const std::string &) {
            return decodeXpm(data, size);
        }
    };
}
