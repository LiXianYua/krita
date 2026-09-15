// R-77 · sdk/tests/compat/kaboutdata.h —— **空转发头**
// ---------------------------------------------------------------------------
// 这是本 Task 唯一一个「实现为空」的交付物，理由必须写清（brief §3.1 末列特别
// 要求「头注写明为什么空是对的」）：
//
// **为什么空是对的：**
//   用量表（impact-map §2）实测：`kaboutdata.h` 在 25 个 `<filestest.h>` 消费者
//   ＋ `filestest.h` 自身里的**唯一**一次出现，就是 `#include <kaboutdata.h>`
//   这一行本身（`filestest.h:51`）——**没有任何函数体使用 KAboutData**。
//   **BASE 树**（`15d11030…`，排除 build 目录）`git grep -in kaboutdata` 命中数 = 1
//   （就是那一行）。注：HEAD 上这个数是 10（新增的本文件自身 8 行 + `filestest.h`
//   的端口化注释引用），「= 1」只在 BASE 口径下成立（Task 3b N-9 订正）。
//   真实 Qt/KF5 里这个头由 KCoreAddons 提供，内容与本调用点无关；本垫片只需
//   **存在且为空**，让那一行成为真正的 no-op。
//
// **它当前没有消费者（已登记的后果）：**
//   brief §3.3 同时要求「删掉 4 个零使用的 include：QTemporaryFile、QApplication、
//   kaboutdata.h、klocalizedstring.h」，并要求「Qt 栈的路径逐字不变」——两条一起
//   只能靠 `#ifndef KRITA_TESTSDK_PK_NATIVE` 包住那 4 行（同 `filestest.h:43-46`
//   对 `testui.h` 的既有做法）。于是端口化之后 **pk 栈不会再 include 本头**。
//   本文件仍然交付，是因为它是 brief §3.1 的**编号交付物**（「新建 5 个垫片」），
//   也是对 `#include <kaboutdata.h>` 在 pk 栈上的**唯一**合法解析目标——若将来
//   有真实调用点（或 §3.3 的清理被回退），不需要再补一次。
//   这条张力已登记在 task1-report.md **§11 第 5 条**（「缺口 / 取舍登记」）。
//   （修复轮 1 订正：旧稿写的「§5」在该报告里是「判据⑤ 改动路径闭包」，
//    指向不存在；Task 3b N-10。）
//
// 若将来出现真实调用点，这里应换成 KAboutData 的最小垫片——**先报再补**
// （判据①「一项不多一项不少」，与 impact-map §2 末的纪律一致）。
// ---------------------------------------------------------------------------
#pragma once
