// R-08 Task 7a：graft driver 的运行期证据探针。
//
// 由 graft_run.sh 的 run_one() 在生成 KisSharedPtrTest 的 driver.cpp 时整段
// cat 进去（见 run_one 里 probe_file 那个可选形参）——这份文件本身不是产物,
// 只是 driver.cpp 生成过程的一段"素材"，测试源（libs/image/tests/
// kis_shared_ptr_test.{h,cpp}）一个字节没改。
//
// 要压的证据缺口（上一轮评审）：libs/global/kis_shared_ptr.h 的 7 处
// `warnKrita.noquote() << kisBacktrace();`（.data() ×2、constData() ×1、
// operator*() ×2、operator->() ×2，对应源文件行号 :314 :326 :338 :359 :368
// :377 :386）与 libs/global/kis_pinned_shared_ptr.h:64-68 的用户 QDebug
// operator<< ——Task 6b 只做到编译期语法验证，这 8 处运行期一次都没被真正
// 调用到。本探针让它们都真的执行一次，并用一个 capture sink 数实际产出的
// 日志条数，断言"不崩 + sink 真收到了"。
//
// 为什么探针要挂在 main() 里（driver.cpp 顶部把 main 宏定义成
// pk_r08_original_main，测试源展开出的 SIMPLE_TEST_MAIN 因此改名，不冲突）
// 而不是塞进本文件自己的全局对象构造函数：PkLogAddSink 背后的注册表
// （PkLogSink.cpp 里 anonymous namespace 的 g_mutex/g_sinks，跨翻译单元的
// non-local static）动态初始化顺序在 C++ 标准里没有跨 TU 顺序保证；标准唯一
// 钉死的是"main() 开始执行前，程序里全部 non-local static 的动态初始化均已
// 完成"。探针若挂在全局构造函数里，有被链接顺序坑到、在 g_mutex 真正构造
// 完成前就调用 PkLogAddSink 的风险——挂在 main() 里就没有这个问题。

namespace {

struct R08CapturedLog {
    PkLogLevel level;
    std::string category;
    std::string message;
};

std::vector<R08CapturedLog> &r08CapturedLogs()
{
    static std::vector<R08CapturedLog> logs;
    return logs;
}

void r08CaptureSink(PkLogLevel level, const PkLogContext &ctx, const char *message, void *)
{
    r08CapturedLogs().push_back(R08CapturedLog{
        level, ctx.category ? ctx.category : "", message ? message : ""});
}

[[noreturn]] void r08Fail(const std::string &what)
{
    std::fprintf(stderr, "R08_PROBE_FAIL: %s\n", what.c_str());
    std::exit(1);
}

// 构造一个失效的弱指针——与真实测试 testBoolOnInvalidWeakPointer() 同一套
// 手法（同一个 TestClass/TestClassWatcher/TestClassSP/TestClassWSP，driver.cpp
// 已经把整份改名后的测试源 #include 进同一个翻译单元，这里直接复用，不重新
// 声明类型）：strong 指针出作用域后 instance 被删，weak.d 仍非空——
// KisWeakSharedPtr::isConsistent()/isValid() 在 d!=0 时公式相同，都会变 false，
// 命中 .data()/constData()/operator*()/operator->() 里 `!isConsistent()` 或
// `!isValid()` 的分支。
void r08ProbeInvalidWeakPtr()
{
    TestClassWatcher tcw;
    TestClass *instance = new TestClass(&tcw);
    TestClassWSP weak(instance);
    {
        TestClassSP strong(instance); // ref，出块即 deref 到 0，触发 ~TestClass()
    }

    if (weak.isValid()) {
        r08Fail("弱指针未能构造成失效状态，探针前提不成立（isValid() 仍为 true）");
    }

    const TestClassWSP &cweak = weak;

    // 下面 7 个调用逐一对应 kis_shared_ptr.h 的 7 处 warnKrita.noquote() <<
    // kisBacktrace()。kisBacktrace() 在 HAVE_BACKTRACE 0 的 stub 下返回空
    // PkString——这正是要压的点：warnKrita.noquote() << 空串必须不崩。
    // Q_ASSERT_X 在本仓库全部 graft 编译行统一带的 -DQT_NO_DEBUG 下退化成
    // qt_noop()（pk/log/tests/graft/stubs/QtGlobal），所以这条分支走完不会
    // 中止进程——这也如实复刻了真实 Krita release 构建（NDEBUG）下同一段代码
    // 的行为，不是探针专属的特殊路径。
    (void)weak.data();           // kis_shared_ptr.h:314 —— T* data()
    (void)cweak.data();          // kis_shared_ptr.h:326 —— const T* data() const
    (void)weak.constData();      // kis_shared_ptr.h:338 —— constData()
    (void)cweak.operator*();     // kis_shared_ptr.h:359 —— operator*() const
    (void)weak.operator*();      // kis_shared_ptr.h:368 —— operator*() 非 const
    (void)cweak.operator->();    // kis_shared_ptr.h:377 —— operator->() const
    (void)weak.operator->();     // kis_shared_ptr.h:386 —— operator->() 非 const
}

// 触发 kis_pinned_shared_ptr.h:64-68 的用户 QDebug operator<<。用一个*有效*
// 指针（与上面失效指针的场景分开）——operator<< 内部调的是 KisSharedPtr::
// data()（kis_shared_ptr.h:139/146，无一致性检查的那一版），有效指针足够。
void r08ProbePinnedOperatorShl()
{
    TestClassWatcher tcw2;
    TestClass *instance2 = new TestClass(&tcw2);
    TestClassSP strong2(instance2);
    KisPinnedSharedPtr<TestClass> pinned(strong2);

    warnKrita.noquote() << pinned;
}

} // namespace

int main(int argc, char **argv)
{
    const int sinkHandle = PkLogAddSink(&r08CaptureSink, nullptr);

    r08ProbeInvalidWeakPtr();
    r08ProbePinnedOperatorShl();

    PkLogRemoveSink(sinkHandle);

    // 断言值先跑一遍看实际条数再定——7 处 warnKrita(弱指针) + 1 处
    // warnKrita(pinned operator<<) = 8，全部落在 krita.general/Warning。
    const std::size_t got = r08CapturedLogs().size();
    const std::size_t want = 8;
    if (got != want) {
        std::fprintf(stderr,
            "R08_PROBE_FAIL: 期望 sink 收到 %zu 条日志，实收 %zu 条：\n", want, got);
        for (const auto &e : r08CapturedLogs()) {
            std::fprintf(stderr, "  level=%d category=%s message=%s\n",
                          static_cast<int>(e.level), e.category.c_str(), e.message.c_str());
        }
        return 1;
    }
    for (const auto &e : r08CapturedLogs()) {
        if (e.category != "krita.general") {
            r08Fail("捕获到的日志分类不是 krita.general：" + e.category);
        }
        if (e.level != PkLogWarning) {
            r08Fail("捕获到的日志级别不是 Warning：" + std::to_string(static_cast<int>(e.level)));
        }
    }

    std::fprintf(stderr,
        "R08_PROBE_OK: 运行期探针通过——sink 捕获 %zu 条 krita.general/Warning 日志"
        "（7 处 warnKrita(弱指针 .data()/constData()/operator*/operator->)"
        " + 1 处 kis_pinned_shared_ptr.h 用户 operator<<），进程未崩溃\n", got);

    return pk_r08_original_main(argc, argv);
}
