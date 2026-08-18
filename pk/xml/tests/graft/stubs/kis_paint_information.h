#pragma once
// `kis_distance_information_test.cpp:17` 用裸文件名 `#include "kis_paint_information.h"`
// （不带 `brushengine/` 前缀）引用同一个头——真实调用点两种写法都有
// （kis_distance_information.cpp 走 `<brushengine/kis_paint_information.h>`，
// 测试 .cpp 走裸名），转发到同一份 stubs/brushengine/kis_paint_information.h，
// 不重复定义一份。
#include "brushengine/kis_paint_information.h"
