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
inline constexpr PathCommand large_positive_origin[] = {
 {PathVerb::MoveTo,40003.25,40003.5,0,0,0,0},{PathVerb::LineTo,40018.75,40003.5,0,0,0,0},{PathVerb::LineTo,40018.75,40018.25,0,0,0,0},{PathVerb::LineTo,40003.25,40018.25,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand large_negative_origin[] = {
 {PathVerb::MoveTo,-40020.75,-40020.5,0,0,0,0},{PathVerb::LineTo,-40005.25,-40020.5,0,0,0,0},{PathVerb::LineTo,-40005.25,-40005.75,0,0,0,0},{PathVerb::LineTo,-40020.75,-40005.75,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand binary_cubic_shared_vertex[] = {
 {PathVerb::MoveTo,-2.5,17.25,0,0,0,0},{PathVerb::CubicTo,5,-9,18,40,36.5,11.75},{PathVerb::CubicTo,28,8,22,8,18,17.25},{PathVerb::CubicTo,12,29,5,29,-2.5,17.25},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand open_and_degenerate[] = {
 {PathVerb::MoveTo,2.5,3.5,0,0,0,0},{PathVerb::LineTo,21.5,3.5,0,0,0,0},
 {PathVerb::MoveTo,7.25,7.5,0,0,0,0},{PathVerb::LineTo,18.5,8.25,0,0,0,0},{PathVerb::LineTo,12.25,21.5,0,0,0,0}};
inline constexpr PathCommand dense_multicontour[] = {
 {PathVerb::MoveTo,1.5,2.5,0,0,0,0},{PathVerb::LineTo,30.5,29.5,0,0,0,0},{PathVerb::LineTo,2.5,25.5,0,0,0,0},{PathVerb::LineTo,29.5,5.5,0,0,0,0},{PathVerb::LineTo,5.5,30.5,0,0,0,0},{PathVerb::LineTo,25.5,1.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0},
 {PathVerb::MoveTo,4.5,1.5,0,0,0,0},{PathVerb::LineTo,27.5,30.5,0,0,0,0},{PathVerb::LineTo,1.5,18.5,0,0,0,0},{PathVerb::LineTo,30.5,13.5,0,0,0,0},{PathVerb::LineTo,3.5,7.5,0,0,0,0},{PathVerb::LineTo,28.5,24.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0},
 {PathVerb::MoveTo,2.5,4.5,0,0,0,0},{PathVerb::LineTo,29.5,27.5,0,0,0,0},{PathVerb::LineTo,3.5,23.5,0,0,0,0},{PathVerb::LineTo,28.5,8.5,0,0,0,0},{PathVerb::LineTo,6.5,29.5,0,0,0,0},{PathVerb::LineTo,24.5,2.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0},
 {PathVerb::MoveTo,5.5,2.5,0,0,0,0},{PathVerb::LineTo,26.5,29.5,0,0,0,0},{PathVerb::LineTo,2.5,16.5,0,0,0,0},{PathVerb::LineTo,29.5,15.5,0,0,0,0},{PathVerb::LineTo,4.5,8.5,0,0,0,0},{PathVerb::LineTo,27.5,23.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0},
 {PathVerb::MoveTo,3.5,3.5,0,0,0,0},{PathVerb::LineTo,28.5,28.5,0,0,0,0},{PathVerb::LineTo,1.5,21.5,0,0,0,0},{PathVerb::LineTo,30.5,10.5,0,0,0,0},{PathVerb::LineTo,7.5,30.5,0,0,0,0},{PathVerb::LineTo,23.5,1.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0},
 {PathVerb::MoveTo,6.5,1.5,0,0,0,0},{PathVerb::LineTo,25.5,30.5,0,0,0,0},{PathVerb::LineTo,1.5,14.5,0,0,0,0},{PathVerb::LineTo,30.5,17.5,0,0,0,0},{PathVerb::LineTo,2.5,9.5,0,0,0,0},{PathVerb::LineTo,29.5,22.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0},
 {PathVerb::MoveTo,9.5,9.5,0,0,0,0},{PathVerb::LineTo,22.5,9.5,0,0,0,0},{PathVerb::LineTo,22.5,22.5,0,0,0,0},{PathVerb::LineTo,9.5,22.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand int_max_origin[] = {
 {PathVerb::MoveTo,2147483634.25,2147483634.5,0,0,0,0},{PathVerb::LineTo,2147483645.75,2147483634.5,0,0,0,0},{PathVerb::LineTo,2147483645.75,2147483645.25,0,0,0,0},{PathVerb::LineTo,2147483634.25,2147483645.25,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand int_min_origin[] = {
 {PathVerb::MoveTo,-2147483646.75,-2147483646.5,0,0,0,0},{PathVerb::LineTo,-2147483635.25,-2147483646.5,0,0,0,0},{PathVerb::LineTo,-2147483635.25,-2147483635.75,0,0,0,0},{PathVerb::LineTo,-2147483646.75,-2147483635.75,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand mirrored_h[] = {
 {PathVerb::MoveTo,24.75,5.5,0,0,0,0},{PathVerb::CubicTo,24,24,12,1,7.25,19.5},{PathVerb::LineTo,24.75,19.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand mirrored_v[] = {
 {PathVerb::MoveTo,7.25,26.5,0,0,0,0},{PathVerb::CubicTo,8,8,20,31,24.75,12.5},{PathVerb::LineTo,7.25,12.5,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
inline constexpr PathCommand open_line[] = {
 {PathVerb::MoveTo,7.25,29.5,0,0,0,0},{PathVerb::LineTo,36.75,8.5,0,0,0,0}};
inline constexpr PathCommand open_curve[] = {
 {PathVerb::MoveTo,5.25,32.5,0,0,0,0},{PathVerb::CubicTo,8.5,1.75,34.5,3.25,38.25,30.5}};
inline constexpr PathCommand clipped_horizontal[] = {
 {PathVerb::MoveTo,-10,10.5,0,0,0,0},{PathVerb::LineTo,54,10.5,0,0,0,0}};
inline constexpr PathCommand acute_join[] = {
 {PathVerb::MoveTo,5.25,34.5,0,0,0,0},{PathVerb::LineTo,22.25,5.5,0,0,0,0},{PathVerb::LineTo,27.75,35.25,0,0,0,0}};
inline constexpr PathCommand closed_triangle[] = {
 {PathVerb::MoveTo,7.25,34.5,0,0,0,0},{PathVerb::LineTo,22.25,5.5,0,0,0,0},{PathVerb::LineTo,37.75,34.25,0,0,0,0},{PathVerb::Close,0,0,0,0,0,0}};
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
 PR_CASE("large_positive_origin_aa",RasterMode::Fill,0,true,40000,40000,24,24,1,1,0,0,4,0,nullptr,0,large_positive_origin),
 PR_CASE("large_negative_origin_aa",RasterMode::Fill,0,true,-40024,-40024,24,24,1,1,0,0,4,0,nullptr,0,large_negative_origin),
 PR_CASE("negative_y_binary_cubic",RasterMode::Fill,1,false,-4,-8,40,40,1,1,0,0,4,0,nullptr,0,binary_cubic_shared_vertex),
 PR_CASE("binary_open_degenerate",RasterMode::Fill,0,false,0,0,24,24,1,1,0,0,4,0,nullptr,0,open_and_degenerate),
 PR_CASE("binary_dense_multicontour",RasterMode::Fill,0,false,0,0,32,32,1,1,0,0,4,0,nullptr,0,dense_multicontour),
 PR_CASE("int_max_origin_binary",RasterMode::Fill,0,false,2147483632,2147483632,16,16,1,1,0,0,4,0,nullptr,0,int_max_origin),
 PR_CASE("int_min_origin_aa",RasterMode::Fill,0,true,-2147483648,-2147483648,16,16,1,1,0,0,4,0,nullptr,0,int_min_origin),
 PR_CASE("stroke_flat_1",RasterMode::Stroke,0,true,0,0,32,32,1,1,0,0,4,0,nullptr,0,open_cubic),
 PR_CASE("stroke_square_3_5",RasterMode::Stroke,0,true,0,0,32,32,3.5,1,16,64,4,0,nullptr,0,open_cubic),
 PR_CASE("stroke_round_dash_8",RasterMode::Stroke,0,true,0,0,32,32,8,6,32,128,4,0.5,dash,2,open_cubic),
 PR_CASE("dash_curve_flat",RasterMode::Stroke,0,true,0,0,32,32,8,6,0,128,4,0.5,dash,2,open_cubic),
 PR_CASE("dash_curve_square",RasterMode::Stroke,0,true,0,0,32,32,8,6,16,128,4,0.5,dash,2,open_cubic),
 PR_CASE("stroke_miter",RasterMode::Stroke,0,true,0,0,32,32,3.5,1,0,0,4,0,nullptr,0,crossing),
 PR_CASE("stroke_bevel",RasterMode::Stroke,0,true,0,0,32,32,3.5,1,0,64,4,0,nullptr,0,crossing),
 PR_CASE("cap_flat",RasterMode::Stroke,0,true,0,0,44,44,8,1,0,64,2,0,nullptr,0,open_curve),
 PR_CASE("cap_square",RasterMode::Stroke,0,true,0,0,44,44,8,1,16,64,2,0,nullptr,0,open_curve),
 PR_CASE("cap_round",RasterMode::Stroke,0,true,0,0,44,44,8,1,32,64,2,0,nullptr,0,open_curve),
 PR_CASE("join_miter_limit_1",RasterMode::Stroke,0,true,0,0,44,44,8,1,0,0,1,0,nullptr,0,acute_join),
 PR_CASE("join_miter_limit_8",RasterMode::Stroke,0,true,0,0,44,44,8,1,0,0,8,0,nullptr,0,acute_join),
 PR_CASE("join_svg_miter_limit_1",RasterMode::Stroke,0,true,0,0,44,44,8,1,0,256,1,0,nullptr,0,acute_join),
 PR_CASE("join_bevel",RasterMode::Stroke,0,true,0,0,44,44,8,1,0,64,2,0,nullptr,0,acute_join),
 PR_CASE("join_round",RasterMode::Stroke,0,true,0,0,44,44,8,1,0,128,2,0,nullptr,0,acute_join),
 PR_CASE("style_solid",RasterMode::Stroke,0,true,0,0,44,44,3.5,1,0,64,2,0,nullptr,0,open_curve),
 PR_CASE("style_dash",RasterMode::Stroke,0,true,0,0,44,44,3.5,2,0,64,2,0,nullptr,0,open_curve),
 PR_CASE("style_dot",RasterMode::Stroke,0,true,0,0,44,44,3.5,3,0,64,2,0,nullptr,0,open_curve),
 PR_CASE("style_dash_dot",RasterMode::Stroke,0,true,0,0,44,44,3.5,4,0,64,2,0,nullptr,0,open_curve),
 PR_CASE("style_dash_dot_dot",RasterMode::Stroke,0,true,0,0,44,44,3.5,5,0,64,2,0,nullptr,0,open_curve),
 PR_CASE("style_custom_offset",RasterMode::Stroke,0,true,0,0,44,44,3.5,6,0,64,2,0.5,dash,2,open_curve),
 PR_CASE("wide_dash_binary",RasterMode::Stroke,0,false,0,0,44,44,3.5,6,0,64,2,0.5,dash,2,open_curve),
 PR_CASE("zero_width_cosmetic",RasterMode::Stroke,0,true,0,0,44,44,0,1,0,64,2,0,nullptr,0,clipped_horizontal),
 PR_CASE("unit_width",RasterMode::Stroke,0,true,0,0,44,44,1,1,0,64,2,0,nullptr,0,clipped_horizontal),
 PR_CASE("cosmetic_half_binary",RasterMode::Stroke,0,false,0,0,44,44,0.5,1,16,64,2,0,nullptr,0,open_curve),
 PR_CASE("cosmetic_half_aa",RasterMode::Stroke,0,true,0,0,44,44,0.5,1,16,64,2,0,nullptr,0,open_curve),
 PR_CASE("cosmetic_dash_flat",RasterMode::Stroke,0,true,0,0,44,44,1,6,0,64,2,0.5,dash,2,open_curve),
 PR_CASE("cosmetic_dash_flat_binary",RasterMode::Stroke,0,false,0,0,44,44,1,6,0,64,2,0.5,dash,2,open_curve),
 PR_CASE("cosmetic_dash_offset_pos_limit",RasterMode::Stroke,0,false,0,0,44,44,1,6,0,64,2,32000000.0,dash,2,open_curve),
 PR_CASE("cosmetic_dash_offset_neg_limit",RasterMode::Stroke,0,false,0,0,44,44,1,6,0,64,2,-32000000.0,dash,2,open_curve),
 PR_CASE("cosmetic_dash_square",RasterMode::Stroke,0,true,0,0,44,44,1,6,16,64,2,0.5,dash,2,open_curve),
 PR_CASE("cosmetic_closed_dash",RasterMode::Stroke,0,true,0,0,44,44,1,6,32,128,2,0.5,dash,2,closed_triangle),
 PR_CASE("cosmetic_large_origin_aa",RasterMode::Stroke,0,true,40000,40000,24,24,1,1,16,64,2,0,nullptr,0,large_positive_origin),
 PR_CASE("closed_flat_cap",RasterMode::Stroke,0,true,0,0,44,44,8,1,0,64,2,0,nullptr,0,closed_triangle),
 PR_CASE("closed_round_cap",RasterMode::Stroke,0,true,0,0,44,44,8,1,32,64,2,0,nullptr,0,closed_triangle)
};
#undef PR_CASE
} // namespace path_rasterizer_cases_detail

inline const std::vector<RasterCase> &pathRasterizerCases()
{ return path_rasterizer_cases_detail::cases; }
