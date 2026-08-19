#pragma once
// 测试本地 shim：QRgb / qRed / qGreen / qBlue / qAlpha / qRgb / qRgba。
//
// 这不是 R-15 的交付物，是「待认领缺口⑤」的临时脚手架。QRgb 与 qRed/qGreen/
// qBlue/qAlpha/qRgb 是 Qt 的**颜色辅助**（qrgb.h），不是 QImage 的方法——它们
// 不在 R-15 用量表的 QImage 方法清单里（判据①，一项不多），pk 里没有对应物
// （PkImage.cpp 只有私有匿名命名空间里的 argbRed/argbGreen/argbBlue 等，语义
// 相同但不对外）。compareQImagesImpl 这个真实调用点需要它们，driver 用本 shim
// 提供，定义逐字照抄真 Qt 5.15.7 的 qrgb.h（QT_VERSION_STR "5.15.7"）：
//
//   typedef unsigned int QRgb;
//   qRed(rgb)   = ((rgb >> 16) & 0xff)
//   qGreen(rgb) = ((rgb >> 8) & 0xff)
//   qBlue(rgb)  = (rgb & 0xff)
//   qAlpha(rgb) = rgb >> 24          // ⚠ 没有 & 0xff，照抄 Qt
//   qRgb(r,g,b) = (0xffu<<24) | ((r&0xffu)<<16) | ((g&0xffu)<<8) | (b&0xffu)
//
// 归宿：未来 PkColor / QRgb 替代品任务，或 S 线按点替换时统一提供。R-15 不
// 该实现它（不是 QImage 方法），所以在这里登记、不并入 PkImage。
#include <cstdint>

typedef unsigned int QRgb;   // RGB triplet（Qt qrgb.h:49）

static inline int qRed(QRgb rgb)   { return ((rgb >> 16) & 0xff); }
static inline int qGreen(QRgb rgb) { return ((rgb >> 8) & 0xff); }
static inline int qBlue(QRgb rgb)  { return (rgb & 0xff); }
static inline int qAlpha(QRgb rgb) { return rgb >> 24; }

static inline QRgb qRgb(int r, int g, int b)
{ return (0xffu << 24) | ((r & 0xffu) << 16) | ((g & 0xffu) << 8) | (b & 0xffu); }

static inline QRgb qRgba(int r, int g, int b, int a)
{ return ((a & 0xffu) << 24) | ((r & 0xffu) << 16) | ((g & 0xffu) << 8) | (b & 0xffu); }
