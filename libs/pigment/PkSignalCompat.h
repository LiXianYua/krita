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

// signals 在 Qt 语义里固定是 public（外部 connect 要取信号地址），moc 头
// （KoColorSet.h 等）的信号段靠它展开才能解析。pk/test/compat 的对象垫片为测试类
// 的 `private Q_SLOTS:` 场景把 signals 预定义成空宏——那对生产头是错的：空展开后
// `signals:` 变成孤立冒号、声明直接解析失败。生产头只认 public，测试类不写裸
// `signals:`，因此这里强制覆盖任何既有定义（先 #undef 再 #define，不触发
// -Wmacro-redefined）。该覆盖不引入任何大写 Q 开头标识符，S 线 Q* 清零判据不受影响。
#ifdef signals
#undef signals
#endif
#define signals public

#ifndef emit
#define emit
#endif

#endif /* PK_SIGNAL_COMPAT_H */
