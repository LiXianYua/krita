// R-77 · sdk/tests/compat/kaboutdata.h —— **空转发头**
// ---------------------------------------------------------------------------
// 这是本 Task 唯一一个「实现为空」的交付物，理由必须写清（brief §3.1 末列特别
// 要求「头注写明为什么空是对的」）：
//
// **为什么空是对的：**
//   用量表（impact-map §2）实测：`kaboutdata.h` 在 25 个 `<filestest.h>` 消费者
//   ＋ `filestest.h` 自身里的**唯一**一次出现，就是 `#include <kaboutdata.h>`
//   这一行本身（`filestest.h:51`）——**没有任何函数体使用 KAboutData**。全树
//   `git grep -in kaboutdata` 命中数 = 1（就是那一行）。
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
//   这条张力已登记在 task1-report.md §5「没做到 / 有出入的地方」。
//
// 若将来出现真实调用点，这里应换成 KAboutData 的最小垫片——**先报再补**
// （判据①「一项不多一项不少」，与 impact-map §2 末的纪律一致）。
// ---------------------------------------------------------------------------
#pragma once
