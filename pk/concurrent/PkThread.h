#pragma once
#include <thread>

// 替代 <QThread> 里保留范围内确有真实调用点的静态方法 + R-24 新增的线程身份 API。
// QThread 的其余用法（子类化、->thread() 亲和性查询）与 moveToThread/PkObject
// 生命周期耦合，本任务不实现，见 Task 5 缺口登记。
//
// 用 std::thread::id 作为线程身份，不新建一个 QThread 平行的对象模型——
// 24 处真实 moveToThread 调用点实测全部是"把亲和性转去一个已经存在的线程"
// （15 处转去 GUI/主线程，9 处转去匹配另一个已有对象所在的线程），零处
// 需要"新建 worker 线程对象、启动它自己的事件循环"，std::thread::id 这个
// 值类型足以承载全部真实语义（见 docs/superpowers/plans/R-24.md「实测优先」）。
using PkThreadId = std::thread::id;

class PkThread {
public:
    static int idealThreadCount();

    // 当前调用线程的身份。
    static PkThreadId currentThreadId();

    // 把调用它的线程注册为"主线程"（对应 Qt 的 qApp->thread()/
    // QCoreApplication::instance()->thread()，真实调用点里 15/24 处
    // moveToThread 转的就是这个身份）。幂等：同一线程重复调用安全；
    // 换一个不同线程调用是编程错误（这里没有 QApplication 对象来天然
    // 界定"唯一一个主线程"，改由调用方显式声明，assert 拦截误用）。
    static void registerMainThread();

    // 已注册的主线程身份；未注册时返回默认构造的 PkThreadId{}
    // （不等于任何 std::thread 实际产生的 id，因此拿它跟 currentThreadId()
    // 比较在未注册场景下天然判定"不同线程"，不会因为忘记注册而崩溃或
    // 误判"是同一线程"）。
    static PkThreadId mainThreadId();
};
