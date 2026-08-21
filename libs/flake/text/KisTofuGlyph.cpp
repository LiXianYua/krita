/*
 *  SPDX-FileCopyrightText: 2023 Alvin Wong <alvin@alvinhc.com>
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 */

#include <PkXmlCompat.h>

#include "KisTofuGlyph.h"


namespace KisTofuGlyph
{

// PkPainterPath::addPolygon 只收 PkPolygonF，这里把 int 坐标的 PkPolygon 显式转一次
// （真 Qt 里 addPolygon(PkPolygon) 走隐式 PkPolygon->PkPolygonF 转换）。
static PkPolygonF toPolygonF(const PkPolygon &poly)
{
    PkVector<PkPointF> pts;
    for (const PkPoint &pt : poly) {
        pts.push_back(PkPointF(pt.x(), pt.y()));
    }
    return PkPolygonF(pts);
}


// These functions build the PkPainterPath for each hex char using polygons.
// Each char glyph is formed by 3x5 grid of squares constructed from polygons
// wound in the clockwise direction (counterclockwise to subtract).

static inline PkPolygon upperHole()
{
    return {PkVector<PkPoint>{{1, 1}, {1, 2}, {2, 2}, {2, 1}}};
}

static inline PkPolygon lowerHole()
{
    return {PkVector<PkPoint>{{1, 3}, {1, 4}, {2, 4}, {2, 3}}};
}

static inline PkPainterPath hexChar0()
{
    static const PkPainterPath s_path0 = []() {
        const PkPolygon b{PkVector<PkPoint>{{1, 1}, {1, 4}, {2, 4}, {2, 1}}};
        PkPainterPath p;
        p.addRect(0, 0, 3, 5);
        p.addPolygon(toPolygonF(b));
        return p;
    }();
    return s_path0;
}

static inline PkPainterPath hexChar1()
{
    static const PkPainterPath s_path1 = []() {
        PkPainterPath p;
        p.addRect(1, 0, 1, 5);
        return p;
    }();
    return s_path1;
}

static inline PkPainterPath hexChar2()
{
    static const PkPainterPath s_path2 = []() {
        const PkPolygon a{PkVector<PkPoint>{
            {0, 0},
            {3, 0},
            {3, 3},
            {1, 3},
            {1, 4},
            {3, 4},
            {3, 5},
            {0, 5},
            {0, 2},
            {2, 2},
            {2, 1},
            {0, 1},
        }};
        PkPainterPath p;
        p.addPolygon(toPolygonF(a));
        return p;
    }();
    return s_path2;
}

static inline PkPainterPath hexChar3()
{
    static const PkPainterPath s_path3 = []() {
        const PkPolygon a{PkVector<PkPoint>{
            {0, 0},
            {3, 0},
            {3, 5},
            {0, 5},
            {0, 4},
            {2, 4},
            {2, 3},
            {0, 3},
            {0, 2},
            {2, 2},
            {2, 1},
            {0, 1},
        }};
        PkPainterPath p;
        p.addPolygon(toPolygonF(a));
        return p;
    }();
    return s_path3;
}

static inline PkPainterPath hexChar4()
{
    static const PkPainterPath s_path4 = []() {
        const PkPolygon a{PkVector<PkPoint>{
            {0, 0},
            {1, 0},
            {1, 2},
            {2, 2},
            {2, 0},
            {3, 0},
            {3, 5},
            {2, 5},
            {2, 3},
            {0, 3},
        }};
        PkPainterPath p;
        p.addPolygon(toPolygonF(a));
        return p;
    }();
    return s_path4;
}

static inline PkPainterPath hexChar5()
{
    static const PkPainterPath s_path5 = []() {
        // Just mirror a "2".
        PkPainterPath p = hexChar2();
        return PkTransform::fromScale(-1, 1).map(p).toReversed().translated(3, 0);
    }();
    return s_path5;
}

static inline PkPainterPath hexChar6()
{
    static const PkPainterPath s_path6 = []() {
        const PkPolygon a{PkVector<PkPoint>{
            {0, 0},
            {3, 0},
            {3, 1},
            {1, 1},
            {1, 2},
            {3, 2},
            {3, 5},
            {0, 5},
        }};
        PkPainterPath p;
        p.addPolygon(toPolygonF(a));
        p.addPolygon(toPolygonF(lowerHole()));
        return p;
    }();
    return s_path6;
}

static inline PkPainterPath hexChar7()
{
    static const PkPainterPath s_path7 = []() {
        const PkPolygon a{PkVector<PkPoint>{
            {0, 0},
            {3, 0},
            {3, 5},
            {2, 5},
            {2, 1},
            {0, 1},
        }};
        PkPainterPath p;
        p.addPolygon(toPolygonF(a));
        return p;
    }();
    return s_path7;
}

static inline PkPainterPath hexChar8()
{
    static const PkPainterPath s_path8 = []() {
        PkPainterPath p;
        p.addRect(0, 0, 3, 5);
        p.addPolygon(toPolygonF(upperHole()));
        p.addPolygon(toPolygonF(lowerHole()));
        return p;
    }();
    return s_path8;
}

static inline PkPainterPath hexChar9()
{
    static const PkPainterPath s_path9 = []() {
        // Just rotate a "6" upside-down
        PkPainterPath p = hexChar6();
        return PkTransform::fromScale(-1, -1).map(p).translated(3, 5);
    }();
    return s_path9;
}

static inline PkPainterPath hexCharA()
{
    static const PkPainterPath s_pathA = []() {
        const PkPolygon a{PkVector<PkPoint>{
            {0, 0},
            {3, 0},
            {3, 5},
            {2, 5},
            {2, 3},
            {1, 3},
            {1, 5},
            {0, 5},
        }};
        PkPainterPath p;
        p.addPolygon(toPolygonF(a));
        p.addPolygon(toPolygonF(upperHole()));
        return p;
    }();
    return s_pathA;
}

static inline PkPainterPath hexCharB()
{
    static const PkPainterPath s_pathB = []() {
        const PkPolygon a{PkVector<PkPoint>{
            {0, 0},
            {2, 0},
            {2, 1},
            {3, 1},
            {3, 2},
            {2, 2},
            {2, 3},
            {3, 3},
            {3, 4},
            {2, 4},
            {2, 5},
            {0, 5},
        }};
        PkPainterPath p;
        p.addPolygon(toPolygonF(a));
        p.addPolygon(toPolygonF(upperHole()));
        p.addPolygon(toPolygonF(lowerHole()));
        return p;
    }();
    return s_pathB;
}

static inline PkPainterPath hexCharC()
{
    static const PkPainterPath s_pathC = []() {
        const PkPolygon a{PkVector<PkPoint>{
            {0, 0},
            {3, 0},
            {3, 1},
            {1, 1},
            {1, 4},
            {3, 4},
            {3, 5},
            {0, 5},
        }};
        PkPainterPath p;
        p.addPolygon(toPolygonF(a));
        return p;
    }();
    return s_pathC;
}

static inline PkPainterPath hexCharD()
{
    static const PkPainterPath s_pathD = []() {
        const PkPolygon a{PkVector<PkPoint>{
            {0, 0},
            {2, 0},
            {2, 1},
            {1, 1},
            {1, 4},
            {2, 4},
            {2, 5},
            {0, 5},
        }};
        const PkPolygon b{PkVector<PkPoint>{{2, 1}, {3, 1}, {3, 4}, {2, 4}}};
        PkPainterPath p;
        p.addPolygon(toPolygonF(a));
        p.addPolygon(toPolygonF(b));
        return p;
    }();
    return s_pathD;
}

static inline PkPainterPath hexCharE()
{
    static const PkPainterPath s_pathE = []() {
        const PkPolygon a{PkVector<PkPoint>{
            {0, 0},
            {3, 0},
            {3, 1},
            {1, 1},
            {1, 2},
            {3, 2},
            {3, 3},
            {1, 3},
            {1, 4},
            {3, 4},
            {3, 5},
            {0, 5},
        }};
        PkPainterPath p;
        p.addPolygon(toPolygonF(a));
        return p;
    }();
    return s_pathE;
}

static inline PkPainterPath hexCharF()
{
    static const PkPainterPath s_pathF = []() {
        const PkPolygon a{PkVector<PkPoint>{
            {0, 0},
            {3, 0},
            {3, 1},
            {1, 1},
            {1, 2},
            {3, 2},
            {3, 3},
            {1, 3},
            {1, 5},
            {0, 5},
        }};
        PkPainterPath p;
        p.addPolygon(toPolygonF(a));
        return p;
    }();
    return s_pathF;
}

static PkPainterPath getHexChar(unsigned value)
{
    switch (value) {
    case 0x0:
        return hexChar0();
    case 0x1:
        return hexChar1();
    case 0x2:
        return hexChar2();
    case 0x3:
        return hexChar3();
    case 0x4:
        return hexChar4();
    case 0x5:
        return hexChar5();
    case 0x6:
        return hexChar6();
    case 0x7:
        return hexChar7();
    case 0x8:
        return hexChar8();
    case 0x9:
        return hexChar9();
    case 0xA:
        return hexCharA();
    case 0xB:
        return hexCharB();
    case 0xC:
        return hexCharC();
    case 0xD:
        return hexCharD();
    case 0xE:
        return hexCharE();
    case 0xF:
        return hexCharF();
    }
    return {};
}

/**
 * @brief Adds a hex char at the specified row/column to the PkPainterPath.
 */
static void addHexChar(PkPainterPath &p, unsigned value, int row, int col)
{
    PkPainterPath glyph = getHexChar(value);
    glyph.translate(2 + col * 4, 2 + row * 6);
    p.addPath(glyph);
}

/**
 * @brief Gets the hex digit at a place.
 * 
 * @param codepoint
 * @param place 0-base digit index
 * @return the digit
 */
static constexpr unsigned valueAt(const char32_t codepoint, const unsigned place)
{
    return (codepoint >> (place * 4)) & 0xF;
}

/**
 * @brief Creates the frame of a tofu glyph
 */
static inline PkPainterPath makeFrame(const int width)
{
    const int inner = width - 1;
    const PkPolygon a{PkVector<PkPoint>{{0, 0}, {width, 0}, {width, 15}, {0, 15}}};
    const PkPolygon b{PkVector<PkPoint>{{1, 1}, {1, 14}, {inner, 14}, {inner, 1}}};
    PkPainterPath p;
    p.addPolygon(toPolygonF(a));
    p.addPolygon(toPolygonF(b));
    return p;
}

PkPainterPath create(const char32_t codepoint, double height)
{
    // We build the glyph as a 15x15 or 11x15 grid of squares.
    PkPainterPath p;
    if (codepoint > 0xFFFF) {
        // Codepoints outside the BMP need more than 4 digits to display, so we show 6.
        // +---+
        // |01F|
        // |389| => U+1F389
        // +---+
        static const PkPainterPath s_outline15 = makeFrame(15);
        p.addPath(s_outline15);
        addHexChar(p, valueAt(codepoint, 5), 0, 0);
        addHexChar(p, valueAt(codepoint, 4), 0, 1);
        addHexChar(p, valueAt(codepoint, 3), 0, 2);
        addHexChar(p, valueAt(codepoint, 2), 1, 0);
        addHexChar(p, valueAt(codepoint, 1), 1, 1);
        addHexChar(p, valueAt(codepoint, 0), 1, 2);
    } else {
        // +--+
        // |27|
        // |64| => U+2764
        // +--+
        static const PkPainterPath s_outline11 = makeFrame(11);
        p.addPath(s_outline11);
        addHexChar(p, valueAt(codepoint, 3), 0, 0);
        addHexChar(p, valueAt(codepoint, 2), 0, 1);
        addHexChar(p, valueAt(codepoint, 1), 1, 0);
        addHexChar(p, valueAt(codepoint, 0), 1, 1);
    }
    const auto scale = (1. / 15.) * height;
    return PkTransform::fromScale(scale, scale).map(p);
}

} // namespace KisTofuGlyph
