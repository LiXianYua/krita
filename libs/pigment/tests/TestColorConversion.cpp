/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <simpletest.h>
#include "TestColorConversion.h"
#include "KoColorConversions.h"


void TestColorConversion::testRGBHSV()
{
    float r, g, b, h, s, v;

    RGBToHSV(1, 0, 0, &h, &s, &v);
    PK_COMPARE(h, 0.0f);
    PK_COMPARE(s, 1.0f);
    PK_COMPARE(v, 1.0f);

    RGBToHSV(1, 1, 0, &h, &s, &v);
    PK_COMPARE(h, 60.0f);
    PK_COMPARE(s, 1.0f);
    PK_COMPARE(v, 1.0f);

    RGBToHSV(0, 1, 0, &h, &s, &v);
    PK_COMPARE(h, 120.0f);
    PK_COMPARE(s, 1.0f);
    PK_COMPARE(v, 1.0f);

    RGBToHSV(0, 1, 1, &h, &s, &v);
    PK_COMPARE(h, 180.0f);
    PK_COMPARE(s, 1.0f);
    PK_COMPARE(v, 1.0f);

    RGBToHSV(0, 0, 1, &h, &s, &v);
    PK_COMPARE(h, 240.0f);
    PK_COMPARE(s, 1.0f);
    PK_COMPARE(v, 1.0f);

    RGBToHSV(1, 0, 1, &h, &s, &v);
    PK_COMPARE(h, 300.0f);
    PK_COMPARE(s, 1.0f);
    PK_COMPARE(v, 1.0f);

    RGBToHSV(0, 0, 0, &h, &s, &v);
    PK_COMPARE(h, -1.0f);
    PK_COMPARE(s, 0.0f);
    PK_COMPARE(v, 0.0f);

    RGBToHSV(1, 1, 1, &h, &s, &v);
    PK_COMPARE(h, -1.0f);
    PK_COMPARE(s, 0.0f);
    PK_COMPARE(v, 1.0f);

    RGBToHSV(0.5, 0.25, 0.75, &h, &s, &v);
    PK_COMPARE(h, 270.0f);
    PK_COMPARE(s, 0.666667f);
    PK_COMPARE(v, 0.75f);

    HSVToRGB(0, 1, 1, &r, &g, &b);
    PK_COMPARE(r, 1.0f);
    PK_COMPARE(g, 0.0f);
    PK_COMPARE(b, 0.0f);

    HSVToRGB(60, 1, 1, &r, &g, &b);
    PK_COMPARE(r, 1.0f);
    PK_COMPARE(g, 1.0f);
    PK_COMPARE(b, 0.0f);

    HSVToRGB(120, 1, 1, &r, &g, &b);
    PK_COMPARE(r, 0.0f);
    PK_COMPARE(g, 1.0f);
    PK_COMPARE(b, 0.0f);

    HSVToRGB(180, 1, 1, &r, &g, &b);
    PK_COMPARE(r, 0.0f);
    PK_COMPARE(g, 1.0f);
    PK_COMPARE(b, 1.0f);

    HSVToRGB(240, 1, 1, &r, &g, &b);
    PK_COMPARE(r, 0.0f);
    PK_COMPARE(g, 0.0f);
    PK_COMPARE(b, 1.0f);

    HSVToRGB(300, 1, 1, &r, &g, &b);
    PK_COMPARE(r, 1.0f);
    PK_COMPARE(g, 0.0f);
    PK_COMPARE(b, 1.0f);

    HSVToRGB(-1, 0, 0, &r, &g, &b);
    PK_COMPARE(r, 0.0f);
    PK_COMPARE(g, 0.0f);
    PK_COMPARE(b, 0.0f);

    HSVToRGB(-1, 0, 1, &r, &g, &b);
    PK_COMPARE(r, 1.0f);
    PK_COMPARE(g, 1.0f);
    PK_COMPARE(b, 1.0f);

    HSVToRGB(270, 0.666667f, 0.75f, &r, &g, &b);
    PK_COMPARE(r, 0.5f);
    PK_COMPARE(g, 0.25f);
    PK_COMPARE(b, 0.75f);
}

void TestColorConversion::testRGBHSL()
{
    float r, g, b, h, s, l;

    RGBToHSL(1, 0, 0, &h, &s, &l);
    PK_COMPARE(h, 0.0f);
    PK_COMPARE(s, 1.0f);
    PK_COMPARE(l, 0.5f);

    RGBToHSL(1, 1, 0, &h, &s, &l);
    PK_COMPARE(h, 60.0f);
    PK_COMPARE(s, 1.0f);
    PK_COMPARE(l, 0.5f);

    RGBToHSL(0, 1, 0, &h, &s, &l);
    PK_COMPARE(h, 120.0f);
    PK_COMPARE(s, 1.0f);
    PK_COMPARE(l, 0.5f);

    RGBToHSL(0, 1, 1, &h, &s, &l);
    PK_COMPARE(h, 180.0f);
    PK_COMPARE(s, 1.0f);
    PK_COMPARE(l, 0.5f);

    RGBToHSL(0, 0, 1, &h, &s, &l);
    PK_COMPARE(h, 240.0f);
    PK_COMPARE(s, 1.0f);
    PK_COMPARE(l, 0.5f);

    RGBToHSL(1, 0, 1, &h, &s, &l);
    PK_COMPARE(h, 300.0f);
    PK_COMPARE(s, 1.0f);
    PK_COMPARE(l, 0.5f);

    RGBToHSL(0, 0, 0, &h, &s, &l);
    PK_COMPARE(h, -1.0f);
    PK_COMPARE(s, 0.0f);
    PK_COMPARE(l, 0.0f);

    RGBToHSL(1, 1, 1, &h, &s, &l);
    PK_COMPARE(h, -1.0f);
    PK_COMPARE(s, 0.0f);
    PK_COMPARE(l, 1.0f);

    RGBToHSL(0.5, 0.25, 0.75, &h, &s, &l);
    PK_COMPARE(h, 270.0f);
    PK_COMPARE(s, 0.5f);
    PK_COMPARE(l, 0.5f);

    HSLToRGB(0, 1, 0.5, &r, &g, &b);
    PK_COMPARE(r, 1.0f);
    PK_COMPARE(g, 0.0f);
    PK_COMPARE(b, 0.0f);

    HSLToRGB(60, 1, 0.5, &r, &g, &b);
    PK_COMPARE(r, 1.0f);
    PK_COMPARE(g, 1.0f);
    PK_COMPARE(b, 0.0f);

    HSLToRGB(120, 1, 0.5, &r, &g, &b);
    PK_COMPARE(r, 0.0f);
    PK_COMPARE(g, 1.0f);
    PK_COMPARE(b, 0.0f);

    HSLToRGB(180, 1, 0.5, &r, &g, &b);
    PK_COMPARE(r, 0.0f);
    PK_COMPARE(g, 1.0f);
    PK_COMPARE(b, 1.0f);

    HSLToRGB(240, 1, 0.5, &r, &g, &b);
    PK_COMPARE(r, 0.0f);
    PK_COMPARE(g, 0.0f);
    PK_COMPARE(b, 1.0f);

    HSLToRGB(300, 1, 0.5, &r, &g, &b);
    PK_COMPARE(r, 1.0f);
    PK_COMPARE(g, 0.0f);
    PK_COMPARE(b, 1.0f);

    HSLToRGB(-1, 0, 0, &r, &g, &b);
    PK_COMPARE(r, 0.0f);
    PK_COMPARE(g, 0.0f);
    PK_COMPARE(b, 0.0f);

    HSLToRGB(-1, 0, 1, &r, &g, &b);
    PK_COMPARE(r, 1.0f);
    PK_COMPARE(g, 1.0f);
    PK_COMPARE(b, 1.0f);

    HSLToRGB(270, 0.5, 0.5, &r, &g, &b);
    PK_COMPARE(r, 0.5f);
    PK_COMPARE(g, 0.25f);
    PK_COMPARE(b, 0.75f);
}

PK_TEST_GUILESS_MAIN(TestColorConversion)
