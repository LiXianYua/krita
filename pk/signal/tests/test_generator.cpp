// 生成器产物 gen_signals.inc 由 run 脚本预先生成（见 Step 5），本 TU 直接 #include
// 它——信号定义是成员函数定义，必须与使用处同 TU 或在链接期可见。这里 #include
// 让 5 个信号的定义进入本 TU，随后 connect/emit 验证运行期行为。
#include <string>
#include "../PkObject.h"
#include "../PkConnect.h"
#include "test_util.h"
#include "generator_cases/basic_signals.h"

// 生成器产物路径：g++ 直编时默认从 ../build/ 读（产物由 Step 5 预先生成在
// pk/signal/build/）；CMake 构建时由 target_compile_definitions 指到
// build_cmake/gen_signals.inc（见 CMakeLists.txt 的 custom_command）。两个形态
// 都必须真能构建，用宏在两端各给一条正确路径。
#ifndef PK_SIGNAL_GEN_INC
#define PK_SIGNAL_GEN_INC "../build/gen_signals.inc"
#endif
#include PK_SIGNAL_GEN_INC

namespace {
struct GenReceiver : PkObject {
    std::string got;
    int n = 0;
    void onPlain() { got += "p"; }
    void onWithArgs(const char* s, int x) { got += s; n = x; }
    void onInt(int a) { n = a; }
    void onStr(const char* a, const char* b) { got += a; got += b; }
    void onUnnamed(const char* s) { got += s; }
};
}

void run_generator_tests()
{
    GenSender s; GenReceiver r;
    PkObject::connect(&s, &GenSender::plain, &r, &GenReceiver::onPlain);
    s.plain();
    _expect(r.got == "p", "generated plain signal fires");

    PkObject::connect(&s, &GenSender::withArgs, &r, &GenReceiver::onWithArgs);
    s.withArgs("hi", 3);
    _expect(r.got == "phi" && r.n == 3, "generated two-arg signal forwards args");

    // 重载消歧：生成器为两个 overloaded 各自生成定义，QOverload 消歧取地址。
    PkObject::connect(&s, QOverload<int>::of(&GenSender::overloaded), &r, &GenReceiver::onInt);
    PkObject::connect(&s, QOverload<const char*, const char*>::of(&GenSender::overloaded), &r, &GenReceiver::onStr);
    s.overloaded(9);
    _expect(r.n == 9, "overloaded int fires");
    s.overloaded("a", "b");
    _expect(r.got == "phiab", "overloaded str fires");

    // 无名字参数：生成器补名 arg0
    PkObject::connect(&s, &GenSender::unnamed, &r, &GenReceiver::onUnnamed);
    s.unnamed("x");
    _expect(r.got == "phiabx", "unnamed-param signal fires");
}
