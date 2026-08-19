#include "PkFlagsTest.h"

// PkTestBinder<PkFlagsTest> 特化由 pk_test_moc.py 生成（CMake 的
// pk_test_generate 触发）。显式特化必须在 qExec<PkFlagsTest> 实例化前对本 TU
// 可见，所以像 moc 的 `#include moc_X.cpp` 惯例一样直接包进来。
#include "pk_binder_PkFlagsTest.inc"

// 存储类型对齐（探针：默认 int 枚举 → Int 是 signed int；: unsigned → unsigned int）
void PkFlagsTest::testIntStorage() {
    static_assert(std::is_same<ECFlags::Int, int>::value, "signed enum -> int storage");
    static_assert(std::is_same<EUFlags::Int, unsigned int>::value, "unsigned enum -> uint storage");
    PK_COMPARE(int(ECFlags()), 0);
}
void PkFlagsTest::testTestFlag() {
    ECFlags ab = EC::A | EC::B;
    PK_VERIFY(ab.testFlag(EC::A));
    PK_VERIFY(ab.testFlag(EC::B));
    PK_VERIFY(!ab.testFlag(EC::C));
    PK_VERIFY(!ab.testFlag(EC::None));          // testFlag(0) on non-zero = false
    PK_VERIFY(ECFlags().testFlag(EC::None));    // testFlag(0) on zero = true
    PK_VERIFY(!ECFlags(EC::A).testFlag(EC::None));
    PK_VERIFY((EC::A | EC::B).testFlag(EC::AB)); // 多 bit 参数：== flag 语义（AB=0x3 全中）
    PK_VERIFY(!ECFlags(EC::A).testFlag(EC::AB)); // 只中一位，!= flag → false
}
void PkFlagsTest::testSetFlag() {
    ECFlags s = EC::A; s.setFlag(EC::B);
    PK_COMPARE(int(s), 3);
    s.setFlag(EC::A, false);
    PK_COMPARE(int(s), 2);
    s.setFlag(EC::B, false);
    PK_COMPARE(int(s), 0);
}
void PkFlagsTest::testOperators() {
    ECFlags one(EC::A); ECFlags notone = ~one;
    PK_COMPARE(int(notone), -2);                 // ~ 作用于 signed int 存储（探针）
    ECFlags abc = EC::A | EC::B | EC::C;
    PK_COMPARE(int(abc & EC::B), 2);             // operator&(Enum)
    PK_COMPARE(int(abc & 0x2), 2);               // operator&(int)
    PK_COMPARE(int(abc & 0x2u), 2);              // operator&(uint)
    ECFlags ab2 = EC::A | EC::B;
    PK_COMPARE(int(ab2 ^ EC::B), 1);             // operator^(Enum)
    ECFlags xa = ab2; xa ^= EC::B;
    PK_COMPARE(int(xa), 1);                       // operator^=(Enum)
    PK_VERIFY(!ECFlags());                        // operator! on zero
    PK_VERIFY(!(!ECFlags(EC::A)));                // operator! on non-zero
    ECFlags il{EC::A, EC::C};                     // initializer_list ctor
    PK_COMPARE(int(il), 5);
}
void PkFlagsTest::testFreeOperatorOr() {
    ECFlags eoe = EC::A | EC::B;                  // enum|enum（自由 operator|）
    PK_COMPARE(int(eoe), 3);
    ECFlags eof = EC::C | eoe;                    // enum|flags
    PK_COMPARE(int(eof), 7);
}
void PkFlagsTest::testComparison() {
    ECFlags eq1 = EC::A | EC::B, eq2 = EC::A | EC::B, eq3 = EC::C;
    PK_COMPARE(eq1 == eq2, true);
    PK_COMPARE(eq1 != eq3, true);
    PK_COMPARE(eq1 == 0, false);                  // 经 operator Int()
    PK_COMPARE(eq1 != 0, true);
}
void PkFlagsTest::testUnsignedEnum() {
    EUFlags eu = EU::U1; EUFlags neu = ~eu;
    PK_COMPARE(unsigned(neu), 4294967294u);       // 探针：unsigned ~U1 = 0xFFFFFFFE
    EUFlags euhi(EU::UHI);
    PK_VERIFY(euhi.testFlag(EU::UHI));
    PK_COMPARE(unsigned(euhi), 2147483648u);
}

PK_TEST_MAIN(PkFlagsTest)