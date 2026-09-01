#pragma once

#include <cstddef>
#include <cstdint>
#include <vector>

enum class PathVerb : uint8_t { MoveTo, LineTo, CubicTo, Close };
enum class RasterMode : uint8_t { Fill, Stroke };
struct PathCommand { PathVerb verb; double x1, y1, x2, y2, x3, y3; };
struct RasterCase {
    const char *name; RasterMode mode; int fillRule; bool antialiased;
    int clipX, clipY, clipWidth, clipHeight; double penWidth;
    int penStyle, capStyle, joinStyle; double miterLimit, dashOffset;
    const double *dashPattern; std::size_t dashCount;
    const PathCommand *commands; std::size_t commandCount;
};

namespace path_rasterizer_cases_detail {
inline constexpr PathCommand nested[] = {
 {PathVerb::MoveTo,4.25,4,0,0,0,0},{PathVerb::LineTo,28.25,4,0,0,0,0},{PathVerb::LineTo,28.25,28,0,0,0,0},{PathVerb::LineTo,4.25,28,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0},
 {PathVerb::MoveTo,10.5,10,0,0,0,0},{PathVerb::LineTo,22.5,10,0,0,0,0},{PathVerb::LineTo,22.5,22,0,0,0,0},{PathVerb::LineTo,10.5,22,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand rect[] = {
 {PathVerb::MoveTo,3.25,3.5,0,0,0,0},{PathVerb::LineTo,18.75,3.5,0,0,0,0},{PathVerb::LineTo,18.75,18.25,0,0,0,0},{PathVerb::LineTo,3.25,18.25,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand open_cubic[] = {
 {PathVerb::MoveTo,2.5,12.25,0,0,0,0},{PathVerb::LineTo,29.5,12.75,0,0,0,0},{PathVerb::MoveTo,7.5,7.25,0,0,0,0},{PathVerb::CubicTo,8,2,25,2,24.5,8},{PathVerb::CubicTo,24,14,9,16,7.5,7.25},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand crossing[] = {
 {PathVerb::MoveTo,-5.5,13.2,0,0,0,0},{PathVerb::CubicTo,1,-3,12,25,21.5,8.75},{PathVerb::CubicTo,24,4,28,17,34.5,12.5},{PathVerb::LineTo,-5.5,12.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand chunk_boundary[] = {
 {PathVerb::MoveTo,247.25,247.5,0,0,0,0},{PathVerb::LineTo,256.75,247.5,0,0,0,0},{PathVerb::LineTo,256.5,257.25,0,0,0,0},{PathVerb::LineTo,247.25,256.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand mirrored_h[] = {
 {PathVerb::MoveTo,24.75,5.5,0,0,0,0},{PathVerb::CubicTo,24,24,12,1,7.25,19.5},{PathVerb::LineTo,24.75,19.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand mirrored_v[] = {
 {PathVerb::MoveTo,7.25,26.5,0,0,0,0},{PathVerb::CubicTo,8,8,20,31,24.75,12.5},{PathVerb::LineTo,7.25,12.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr double dash[] = {2.0,1.0};
#define PR_CASE(n,m,f,a,x,y,w,h,p,ps,cs,js,ml,doff,dp,dc,cmd) {n,m,f,a,x,y,w,h,p,ps,cs,js,ml,doff,dp,dc,cmd,sizeof(cmd)/sizeof(*cmd)}
inline const std::vector<RasterCase> cases = {
 PR_CASE("nested_oddeven",RasterMode::Fill,0,false,0,0,32,32,1,1,0,0,4,0,nullptr,0,nested),
 PR_CASE("nested_winding",RasterMode::Fill,1,false,0,0,32,32,1,1,0,0,4,0,nullptr,0,nested),
 PR_CASE("aa_off_rect",RasterMode::Fill,0,false,0,0,24,24,1,1,0,0,4,0,nullptr,0,rect),
 PR_CASE("aa_on_rect",RasterMode::Fill,0,true,0,0,24,24,1,1,0,0,4,0,nullptr,0,rect),
 PR_CASE("aa_off_curve",RasterMode::Stroke,0,false,0,0,32,32,1,1,0,0,4,0,nullptr,0,open_cubic),
 PR_CASE("aa_on_curve",RasterMode::Stroke,0,true,0,0,32,32,1,1,0,0,4,0,nullptr,0,open_cubic),
 PR_CASE("open_line_closed_cubic",RasterMode::Stroke,0,true,0,0,32,32,3.5,1,0,0,4,0,nullptr,0,open_cubic),
 PR_CASE("crossing_clip",RasterMode::Fill,0,true,-3,5,19,17,1,1,0,0,4,0,nullptr,0,crossing),
 PR_CASE("mirror_horizontal",RasterMode::Stroke,0,true,0,0,32,32,1,1,16,0,4,0,nullptr,0,mirrored_h),
 PR_CASE("mirror_vertical",RasterMode::Stroke,0,true,0,0,32,32,8,1,32,128,4,0,nullptr,0,mirrored_v),
 PR_CASE("chunk_boundary",RasterMode::Fill,0,false,248,248,16,16,1,1,0,0,4,0,nullptr,0,chunk_boundary),
 PR_CASE("stroke_flat_1",RasterMode::Stroke,0,true,0,0,32,32,1,1,0,0,4,0,nullptr,0,open_cubic),
 PR_CASE("stroke_square_3_5",RasterMode::Stroke,0,true,0,0,32,32,3.5,1,16,64,4,0,nullptr,0,open_cubic),
 PR_CASE("stroke_round_dash_8",RasterMode::Stroke,0,true,0,0,32,32,8,6,32,128,4,0.5,dash,2,open_cubic),
 PR_CASE("stroke_miter",RasterMode::Stroke,0,true,0,0,32,32,3.5,1,0,0,4,0,nullptr,0,crossing),
 PR_CASE("stroke_bevel",RasterMode::Stroke,0,true,0,0,32,32,3.5,1,0,64,4,0,nullptr,0,crossing)
};
#undef PR_CASE
} // namespace path_rasterizer_cases_detail

inline const std::vector<RasterCase> &pathRasterizerCases()
{ return path_rasterizer_cases_detail::cases; }
