#include "test_sink.h"
#include "../PkLogBackend.h"
#include "../PkLogSink.h"
#include <string>
#include <vector>

static std::vector<std::string> g_captured;

static void capture(PkLogLevel level, const PkLogContext &ctx,
                    const char *message, void *)
{
    g_captured.push_back(std::string(ctx.category) + "|" +
                         std::to_string(static_cast<int>(level)) + "|" + message);
}

void PkLogSinkTest::testSinkReceivesMessage()
{
    g_captured.clear();
    const int h = PkLogAddSink(capture, nullptr);
    PkLogEnsureLogger("krita.general", PkLogDebug);
    PkLogEmit(PkLogWarning, PkLogContext{"f.cpp", 7, "fn", "krita.general"}, "hello");
    PkLogRemoveSink(h);
    PK_COMPARE(static_cast<int>(g_captured.size()), 1);
    PK_COMPARE(g_captured[0], std::string("krita.general|2|hello"));
}

void PkLogSinkTest::testRemovedSinkStopsReceiving()
{
    g_captured.clear();
    const int h = PkLogAddSink(capture, nullptr);
    PkLogRemoveSink(h);
    PkLogEmit(PkLogWarning, PkLogContext{"f.cpp", 7, "fn", "krita.general"}, "hello");
    PK_COMPARE(static_cast<int>(g_captured.size()), 0);
}

// PkTestBinder<T> 是显式特化，qExec<T> 实例化处必须与它同一个 TU
// （pk/test/CMakeLists.txt:74-79 的 ODR 硬规则）。
#include "pk_binder_test_sink.inc"

int run_sink_tests(int argc, char **argv)
{
    PkLogSinkTest tc;
    return PkTest::qExec(&tc, argc, argv);
}
