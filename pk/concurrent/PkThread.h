#pragma once

// 替代 <QThread> 里保留范围内确有真实调用点的静态方法。QThread 的其余
// 用法（子类化、->thread() 亲和性查询）与 moveToThread/PkObject 生命周期
// 耦合，本任务不实现，见 Task 5 缺口登记。
class PkThread {
public:
    static int idealThreadCount();
};
