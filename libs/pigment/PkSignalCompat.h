/*
 *  SPDX-FileCopyrightText: 2026 S-03-a
 *
 *  SPDX-License-Identifier: GPL-2.0-or-later
 *
 *  Q*-free 的信号访问垫片：给 moc 头文件（KoColorSet.h / KoColorDisplayRendererInterface.h /
 *  KisUniqueColorSet.h）提供 moc 展开所需的最小宏集，替代 pk/signal/compat 里针对
 *  真 Qt 信号宏的那份兼容头（那份含大写 Q 开头的类型字面量，S 线 Q* 清零判据过不了）。
 *
 *  - signals → public：信号段访问标记。pk_signal_moc.py 的扫描正则识别小写
 *    "signals:" 标记（也接受带 Q 前缀的等价拼写），用它即可驱动信号定义生成，
 *    且不触碰 Q* 清零正则（大写 Q 后跟大写字母才命中，signals 首字母小写）。
 *  - emit → 空：信号发射语义由信号成员函数体内的 activateSignal 承担，调用点不写 emit。
 *
 *  本头自身不含任何大写 Q 开头的标识符，可安全出现在 Q* 清零目标文件里。
 */
#ifndef PK_SIGNAL_COMPAT_H
#define PK_SIGNAL_COMPAT_H

#ifndef signals
#define signals public
#endif

#ifndef emit
#define emit
#endif

#endif /* PK_SIGNAL_COMPAT_H */
