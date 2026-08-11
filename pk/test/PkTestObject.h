#pragma once

// 测试类的基类。Krita 的测试类写的是 `class X : public QObject`，
// compat/QObject 把 QObject 改写成 PkTestObject。
//
// 这**不是** R-05 要交付的 PkObject —— 它没有信号槽、没有对象树、没有属性系统。
// R-11 只需要"测试类有个带虚析构的公共基类，好让 qExec 用 PkTestObject* 传递"。
// R-05 交付真正的对象系统之后，compat/QObject 应改指过去。
class PkTestObject
{
public:
    virtual ~PkTestObject() = default;
};
