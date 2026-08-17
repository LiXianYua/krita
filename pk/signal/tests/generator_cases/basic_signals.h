#pragma once
// 生成器自测输入：覆盖无参、带参、重载、无名字参数四种形态。
// 用 compat 垫片而非直接 PkObject.h：Q_SIGNALS 等宏只有垫片定义，这里顺带
// 验证垫片能在一个「真实 Krita 头」形态的 TU 里生效（`#include <QObject>`
// 语义由 Task 5 的 graft 补，此处以相对路径解析到同一份垫片）。
#include "../../compat/QObject"

class GenSender : public PkObject
{
public:
Q_SIGNALS:
    void plain();
    void withArgs(const char* s, int n);
    void overloaded(int a);
    void overloaded(const char* a, const char* b);
    void unnamed(const char*);
};
