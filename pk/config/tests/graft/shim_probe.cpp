// I-2 最终评审修复：Task 3 的自测直接 #include "../PkMimeDatabase.h"，Task 4
// 的 18 个真实消费者试接全部卡在 KisMimeDatabase 引用之前更上游的未交付类型
// 上（见 graft_check.sh 顶部注释）——两边都没有真正验证过
// compat/KisMimeDatabase.h 这层 #define KisMimeDatabase PkMimeDatabase 间接
// 能不能编、能不能跑对。本探针独立走一遍真实消费者会走的路径：
// 通过 -I pk/config/compat 用尖括号 #include <KisMimeDatabase.h>（不是
// -I pk/config 直接拿 PkMimeDatabase.h 的定义），再调用
// KisMimeDatabase::mimeTypeForSuffix("kpp")，验证宏替换与查表结果都对。
#include <cstdio>
#include <cstdlib>
#include <KisMimeDatabase.h>

int main()
{
    // "kpp" 是 pk/config/PkMimeDatabase.cpp 表里第 27 行的真实条目
    // （application/x-krita-paintoppreset），test_mime_database.cpp 的
    // allSuffixesRoundTripToMimeType 已经核对过整张表；这里只需要证明"经过
    // compat 垫片这层间接"依然能查到同一个结果。
    PkString result = KisMimeDatabase::mimeTypeForSuffix("kpp");
    PkString expected("application/x-krita-paintoppreset");

    if (result == expected) {
        std::printf("PASS: KisMimeDatabase::mimeTypeForSuffix(\"kpp\") via compat shim == %s\n",
                     expected.PkToUtf8().c_str());
        return 0;
    }

    std::printf("FAIL: KisMimeDatabase::mimeTypeForSuffix(\"kpp\") via compat shim returned "
                "unexpected value (expected %s, got %s)\n",
                expected.PkToUtf8().c_str(), result.PkToUtf8().c_str());
    return 1;
}
