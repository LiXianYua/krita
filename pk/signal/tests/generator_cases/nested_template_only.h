#pragma once
// 生成器-only 测试输入：只喂 pk_signal_moc.py 解析，不参与编译。
// QList/QVector/QHash/QPair 在 pk/signal 里没有定义（真实 Krita 才有），这个
// 头不能被任何 .cpp include——它只用于钉死「嵌套模板参数里的内层逗号和 >> 不能
// 被 split_top_level 误切」这一解析行为。对应断言见 tests/selftest_generator.py。
#include "PkObject.h"

class NestedSender : public PkObject
{
public:
Q_SIGNALS:
    // 内层逗号：QHash<int,int> 里的逗号是模板参数分隔，不是信号参数分隔；
    // >>：QList<QVector<int>> 的尾 >> 是两个独立的右尖括号，各自关一层。
    void nestedTemplate(const QList<QVector<int>>& items, const QHash<int,int>& map);
    void nestedVector(const QPair<QVector<int>, QHash<int,int>>& pair);
};