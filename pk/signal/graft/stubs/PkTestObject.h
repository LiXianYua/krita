#pragma once
// 试接脚手架 —— 不是 R-05 的交付物。
//
// 用途：让 R-11 harness 期望的 PkTestObject* 与 R-05 测试类实际的基类 PkObject*
// 是同一个类型。R-11 的 harness（pk/test/PkTest.h 的 execPlan、PkTestBinder 的
// invoke lambda）把测试类都当 PkTestObject* 传递；R-05 交付 PkObject 后测试类
// 改继承 PkObject（compat/QObject 把 QObject 改写为 PkObject），两个基类推给
// 同一份 harness 才会编过。R-11 README §3 已预见此改指（「R-05 交付真正的对象
// 系统之后，compat/QObject 应改指过去」），改 pk/test/PkTestObject.h 是 R-11
// 锁内的事，本试接只做 override。
//
// brief 原方案靠「-I 优先级让 override 的 PkTestObject.h 先被解析」；实测 GCC 的
// 引号 include 规则是「先查当前文件所在目录，再查 -I」，而 pk/test/PkTest.h 顶部
// `#include "PkTestObject.h"` 与其同目录 → 无论 stubs 在 -I 排多靠前都命中真品。
// 本 override 因此改用 `-include` 预包含（见 graft_run.sh 编译行）：在一切翻译
// 单元之前先把真品 PkTestObject.h 完整吃进来并用 #pragma once 记住它的 include
// 状态，随后定义 `PkTestObject → PkObject` 宏；之后任何 `#include "PkTestObject.h"`
// 被 #pragma once 拦截成空，宏在 C++ 代码里把 PkTestObject 改写为 PkObject。
// harness 里的一切 PkTestObject* 实际是 PkObject*，且从未出第二份类定义。
//
// 顺序两条都不可颠倒：
// ① 先 include PkObject.h——宏 `PkTestObject PkObject` 展开后，PkTest.h/binder.inc
//    里的 `PkTestObject *` 会变成 `PkObject *`，此刻 PkObject 必须已声明。
// ② 再 include 真品 PkTestObject.h——必须在宏定义之前，否则真品里的
//    `class PkTestObject {}` 会被宏改写成重复的 `class PkObject {}`。
//
// 用 `#pragma once` 而非传统 include guard：传统 guard 靠头文件自定义宏名去重，
// PkTestObject 没有这样的宏；#pragma once 按文件记状态，跨「预包含 + 之后引号
// include」两种形式都认同一份文件，正好是这里需要的去重语义。
#include "../../PkObject.h"
#include "../../../test/PkTestObject.h"
#define PkTestObject PkObject
