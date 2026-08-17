#pragma once
#include <cstddef>
#include <cstring>
#include <tuple>

// 从成员函数指针提取「所属类」与「参数包」。只特化非 const 非 volatile 的普通
// 成员函数——Krita 的信号声明都是 `void sigXxx(...)`（§6.1 信号是成员函数）。
// const 成员函数指针会落入未定义主模板，编译期报错，正是想要的信号。
template <typename Func>
struct PkSignalTraits;

template <typename Ret, typename C, typename... Args>
struct PkSignalTraits<Ret (C::*)(Args...)>
{
    using Object = C;
    using Return = Ret;
    using ArgsTuple = std::tuple<Args...>;
};

// 成员函数指针打包成可比较的 key。Itanium ABI 下普通（无虚继承）成员函数指针
// 是 2 个 word：代码地址 + this 调整量（ptrdiff_t，不是指针）。Krita 的信号类
// 全是普通继承自 QObject（PkObject），无虚继承，2-word 装得下；出现虚继承会
// 触发 static_assert 编译期报错——那正是想要「响亮失败」而不是静默 key 碰撞的场景。
// 存成字节数组 + memcmp 比较：把第二个 word（this 调整量）当 void* 读并比较是
// 严格标准的 UB，字节级比较则与具体 ABI 无关、无别名/表示假设。
struct PkMemberFnKey
{
    unsigned char words[2 * sizeof(void*)];

    template <typename MFN>
    static PkMemberFnKey from(MFN mfn)
    {
        static_assert(sizeof(MFN) <= sizeof(words),
                      "PkMemberFnKey: member function pointer too large (virtual inheritance?)");
        PkMemberFnKey k{};   // 值初始化全零；memcpy 全量覆盖 sizeof(MFN) 字节，无需再 memset
        std::memcpy(&k, &mfn, sizeof(MFN));
        return k;
    }

    bool operator==(const PkMemberFnKey& o) const
    {
        return std::memcmp(words, o.words, sizeof(words)) == 0;
    }
};

// 连接的生命周期状态。PkConnection（句柄）与连接条目各自持有同一个 shared_ptr，
// disconnect() 或对象析构时把 alive 置 false；emit 遍历时跳过 dead 条目。
// 用 shared_ptr 而非裸 bool：句柄拷贝后仍指同一状态，且对象析构后句柄仍安全。
struct PkConnectionState
{
    bool alive = true;
};
