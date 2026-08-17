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
// 是 2 个 word：代码地址 + this 调整量。Krita 的信号类全是普通继承自 QObject
// （PkObject），无虚继承，2-word 装得下；出现虚继承会触发 static_assert 编译期
// 报错——那正是想要「响亮失败」而不是静默 key 碰撞的场景。
struct PkMemberFnKey
{
    void* words[2];

    template <typename MFN>
    static PkMemberFnKey from(MFN mfn)
    {
        static_assert(sizeof(MFN) <= sizeof(words),
                      "PkMemberFnKey: member function pointer too large (virtual inheritance?)");
        PkMemberFnKey k{};
        std::memset(&k, 0, sizeof(k));
        std::memcpy(&k, &mfn, sizeof(MFN));
        return k;
    }

    bool operator==(const PkMemberFnKey& o) const
    {
        return words[0] == o.words[0] && words[1] == o.words[1];
    }
};

// 连接的生命周期状态。PkConnection（句柄）与连接条目各自持有同一个 shared_ptr，
// disconnect() 或对象析构时把 alive 置 false；emit 遍历时跳过 dead 条目。
// 用 shared_ptr 而非裸 bool：句柄拷贝后仍指同一状态，且对象析构后句柄仍安全。
struct PkConnectionState
{
    bool alive = true;
};
