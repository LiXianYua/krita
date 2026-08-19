// difftest_flags.cpp —— QFlags ↔ PkFlags 逐输入对拍（甲类核心判据）
// 两侧真分别 include <QFlags> 与 PkFlags.h；-I 绝不能给 compat/（否则同型恒等）。
// 组合：16 个位掩码 × 每操作 × 参数（每枚枚举值含 None/多 bit）。校验值 = 真 Qt。
//
// 注意：PK_DECLARE_OPERATORS_FOR_FLAGS 与 Q_DECLARE_OPERATORS_FOR_FLAGS
// 对于同一枚举类型会声明冲突的自由 operator|（相同参数类型、不同返回类型），
// 所以 Pk 侧不用 PK_DECLARE_OPERATORS_FOR_FLAGS，改用 PkFlags 的成员
// operator|(Enum)/operator|(PkFlags)。PkECFlags(EC::A) | EC::B 走的是
// PkFlags<Enum>::operator|(Enum) 成员函数，不与 Qt 的 operator|(EC,EC) 冲突。
#include <QFlags>
#include "PkFlags.h"
#include <cstdio>
#include <string>
#include <type_traits>
#include <map>

enum class EC { None = 0, A = 0x1, B = 0x2, C = 0x4, D = 0x8, AB = 0x3, ABC = 0x7 };
Q_DECLARE_FLAGS(ECFlags, EC)
Q_DECLARE_OPERATORS_FOR_FLAGS(ECFlags)
// Pk 侧用 PkFlags<EC> 直接，不声明自由 operator|（避免冲突）
typedef PkFlags<EC> PkECFlags;

enum PlainE { PE_None = 0, PE_A = 0x1, PE_B = 0x2, PE_C = 0x4 };
Q_DECLARE_FLAGS(PlainFlags, PlainE)
Q_DECLARE_OPERATORS_FOR_FLAGS(PlainFlags)
typedef PkFlags<PlainE> PkPlainFlags;

enum class EU : unsigned int { U0 = 0, U1 = 1u, U2 = 2u, UHI = 0x80000000u };
Q_DECLARE_FLAGS(EUFlags, EU)
Q_DECLARE_OPERATORS_FOR_FLAGS(EUFlags)
typedef PkFlags<EU> PkEUFlags;

static_assert(!std::is_same<ECFlags, PkECFlags>::value, "对拍两侧解析成了同一类型");

static long g_total = 0, g_mismatch = 0;
static std::map<std::string, long> g_tags, g_cover;

static void rec(const std::string &api, bool same, const std::string &tag,
                const std::string &in, long qt, long pk) {
    ++g_total; ++g_cover[api];
    if (same) return;
    ++g_mismatch; ++g_tags[api + " " + tag];
    static int printed = 0;
    if (printed < 40) { ++printed; std::printf("MISMATCH: %s [%s] in=%s qt=%ld pk=%ld\n", api.c_str(), tag.c_str(), in.c_str(), qt, pk); }
}

// tag = <api> <mask-hex> <arg-hex>；由触发差异的输入形态构造（规则一）。
static std::string hex(long v) { char b[32]; std::snprintf(b, sizeof b, "0x%lx", v); return b; }

int main() {
    const EC ecs[] = { EC::None, EC::A, EC::B, EC::C, EC::D, EC::AB, EC::ABC };
    for (int m = 0; m < 16; ++m) {                       // 掩码全组合 0..0xF
        ECFlags q((ECFlags::enum_type)m);                // Qt 侧
        PkECFlags p((EC)m);                              // Pk 侧
        std::string in = hex(m);
        for (EC e : ecs) {                               // testFlag：单枚举参数（含 None/多 bit）
            rec("testFlag", q.testFlag(e) == p.testFlag(e), "testFlag " + in + " " + hex((int)e),
                in, q.testFlag(e), p.testFlag(e));
        }
        for (EC e : ecs) {                               // setFlag on/off
            ECFlags q2 = q; q2.setFlag(e);
            PkECFlags p2 = p; p2.setFlag(e);
            rec("setFlag.on", int(q2) == int(p2), "setFlag.on " + in + " " + hex((int)e), in, int(q2), int(p2));
            ECFlags q3 = q; q3.setFlag(e, false);
            PkECFlags p3 = p; p3.setFlag(e, false);
            rec("setFlag.off", int(q3) == int(p3), "setFlag.off " + in + " " + hex((int)e), in, int(q3), int(p3));
        }
        for (EC e : ecs) {                               // | & ^ ~ 与 int/uint & 重载
            // Pk 侧用成员 operator|(Enum) 避开自由 operator| 冲突
            rec("or", int(q | e) == int(p | e), "or " + in + " " + hex((int)e), in, int(q | e), int(p | e));
            rec("and", int(q & e) == int(p & e), "and " + in + " " + hex((int)e), in, int(q & e), int(p & e));
            rec("xor", int(q ^ e) == int(p ^ e), "xor " + in + " " + hex((int)e), in, int(q ^ e), int(p ^ e));
        }
        rec("not", int(~q) == int(~p), "not " + in, in, int(~q), int(~p));
        rec("bang", (!q) == (!p), "bang " + in, in, (!q), (!p));
        rec("intconv", int(q) == int(p), "intconv " + in, in, int(q), int(p));
        for (int m2 = 0; m2 < 16; ++m2) {                // flags & flags / flags == flags
            ECFlags q2((ECFlags::enum_type)m2);
            PkECFlags p2((EC)m2);
            rec("and.flags", int(q & q2) == int(p & p2), "and.flags " + in + " " + hex(m2), in, int(q & q2), int(p & p2));
            rec("eq", (q == q2) == (p == p2), "eq " + in + " " + hex(m2), in, (q == q2), (p == p2));
        }
    }
    // 高位掩码（signed 0x40000000 + unsigned 0x80000000u）—— 用 PkFlag/QFlag 构造
    // 注意：operator|=(int) 两边都不存在，必须用 QFlag/PkFlag 构造 flags 再或。
    ECFlags qhi((EC::ABC)); qhi = qhi | ECFlags(QFlag(0x40000000));
    PkECFlags phi(EC::ABC); phi = phi | PkECFlags(PkFlag(0x40000000));
    rec("high.signed", int(qhi) == int(phi), "high.signed 0x40000000", "0x40000000", int(qhi), int(phi));
    EUFlags qu(EU::UHI); PkEUFlags pu(EU::UHI);
    rec("high.unsigned", unsigned(qu) == unsigned(pu), "high.unsigned 0x80000000", "0x80000000", unsigned(qu), unsigned(pu));
    // 无作用域 enum 也过一遍 testFlag + or
    for (int m = 0; m < 8; ++m) {
        PlainFlags q((PlainFlags::enum_type)m); PkPlainFlags p((PlainE)m);
        rec("plain.testFlag", q.testFlag(PE_A) == p.testFlag(PE_A), "plain.testFlag " + hex(m), hex(m), q.testFlag(PE_A), p.testFlag(PE_A));
        rec("plain.or", int(q | PE_C) == int(p | PE_C), "plain.or " + hex(m), hex(m), int(q | PE_C), int(p | PE_C));
    }
    for (const auto &kv : g_cover) std::printf("ORACLE-COVER %s %ld\n", kv.first.c_str(), kv.second);
    for (const auto &kv : g_tags) std::printf("DIFFTAG %s %ld\n", kv.first.c_str(), kv.second);
    std::printf("DIFF total=%ld mismatch=%ld\n", g_total, g_mismatch);
    return 0;   // 即使 mismatch>0 也退 0，判定归 reviewer
}