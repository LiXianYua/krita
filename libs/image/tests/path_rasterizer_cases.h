#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum class PathVerb : uint8_t { MoveTo, LineTo, CubicTo, Close };
enum class RasterMode : uint8_t { Fill, Stroke };

struct PathCommand { PathVerb verb; double x1, y1, x2, y2, x3, y3; };
struct RasterCase {
    const char *name;
    RasterMode mode;
    int fillRule;
    bool antialiased;
    int clipX, clipY, clipWidth, clipHeight;
    double penWidth;
    int penStyle, capStyle, joinStyle;
    double miterLimit, dashOffset;
    const double *dashPattern;
    std::size_t dashCount;
    const PathCommand *commands;
    std::size_t commandCount;
};

const std::vector<RasterCase> &pathRasterizerCases();
