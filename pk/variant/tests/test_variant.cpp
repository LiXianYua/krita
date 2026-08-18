#include "variant_case.h"

#include "../PkVariant.h"

#include "pk_binder_variant_case.inc"

#include <cmath>
#include <limits>

// ── 基础状态 ──────────────────────────────────────────────────────────────

void VariantCase::defaultConstruction()
{
    PkVariant v;
    PK_VERIFY(v.isNull());
    PK_VERIFY(!v.isValid());
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Invalid));
    PK_COMPARE(v.userType(), 0);
    PK_COMPARE(v.toInt(), 0);
    PK_COMPARE(v.toBool(), false);
    PK_COMPARE(v.toDouble(), 0.0);
    PK_COMPARE(v.toFloat(), 0.0f);
    PK_COMPARE(v.toLongLong(), 0LL);
    PK_COMPARE(v.toULongLong(), 0ULL);
    PK_COMPARE(v.toUInt(), 0u);
    PK_VERIFY(v.toString().isEmpty());
    PK_VERIFY(v.toByteArray().isEmpty());
    PK_VERIFY(!v.canConvert<int>());
    PK_VERIFY(!v.canConvert<PkString>());
}

void VariantCase::clearReset()
{
    PkVariant v;
    v.clear();
    PK_VERIFY(v.isNull());
    PK_VERIFY(!v.isValid());
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Invalid));
    PK_COMPARE(v.toInt(), 0);

    // setValue then clear
    PkVariant v2(42);
    PK_VERIFY(!v2.isNull());
    v2.clear();
    PK_VERIFY(v2.isNull());
    PK_VERIFY(!v2.isValid());
    PK_COMPARE(static_cast<int>(v2.type()), static_cast<int>(PkVariant::Invalid));
    PK_COMPARE(v2.toInt(), 0);
}

void VariantCase::isValidAndIsNull()
{
    // default
    PkVariant dv;
    PK_VERIFY(dv.isNull());
    PK_VERIFY(!dv.isValid());

    // int 0
    PkVariant i0(0);
    PK_VERIFY(!i0.isNull());
    PK_VERIFY(i0.isValid());

    // int 42
    PkVariant i42(42);
    PK_VERIFY(!i42.isNull());
    PK_VERIFY(i42.isValid());

    // empty string
    PkVariant es("");
    PK_VERIFY(!es.isNull());
    PK_VERIFY(es.isValid());

    // non-empty string
    PkVariant nes("hello");
    PK_VERIFY(!nes.isNull());
    PK_VERIFY(nes.isValid());

    // bool false
    PkVariant bf(false);
    PK_VERIFY(!bf.isNull());
    PK_VERIFY(bf.isValid());

    // NaN
    double nan = std::numeric_limits<double>::quiet_NaN();
    PkVariant nv(nan);
    PK_VERIFY(!nv.isNull());
    PK_VERIFY(nv.isValid());
}

// ── 基础类型构造与转换 ────────────────────────────────────────────────────

void VariantCase::intVariant()
{
    PkVariant v(42);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Int));
    PK_COMPARE(v.userType(), static_cast<int>(PkVariant::Int));
    PK_VERIFY(!v.isNull());
    PK_VERIFY(v.isValid());
    PK_COMPARE(v.toInt(), 42);
    PK_COMPARE(v.toBool(), true);
    PK_COMPARE(v.toDouble(), 42.0);
    PK_COMPARE(v.toFloat(), 42.0f);
    PK_COMPARE(v.toLongLong(), 42LL);
    PK_COMPARE(v.toULongLong(), 42ULL);
    PK_COMPARE(v.toUInt(), 42u);
    PK_VERIFY(v.toReal() == 42.0);
    PK_VERIFY(v.canConvert<int>());
    PK_VERIFY(v.canConvert<double>());
    PK_VERIFY(v.canConvert<PkString>());
    PK_VERIFY(v.canConvert<bool>());
    PK_VERIFY(v.canConvert<float>());
    // PkString toString from int
    PkString ts = v.toString();
    PK_VERIFY(!ts.isEmpty());

    // Int 0
    PkVariant v0(0);
    PK_VERIFY(!v0.isNull());
    PK_VERIFY(v0.isValid());
    PK_COMPARE(v0.toInt(), 0);
    PK_COMPARE(v0.toBool(), false);
    PK_COMPARE(v0.toDouble(), 0.0);
}

void VariantCase::uintVariant()
{
    PkVariant v(42u);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::UInt));
    PK_COMPARE(v.toUInt(), 42u);
    PK_COMPARE(v.toInt(), 42);
    PK_COMPARE(v.toDouble(), 42.0);
}

void VariantCase::longLongVariant()
{
    PkVariant v(42LL);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::LongLong));
    PK_COMPARE(v.toLongLong(), 42LL);
    PK_COMPARE(v.toInt(), 42);
}

void VariantCase::ulongLongVariant()
{
    PkVariant v(42ULL);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::ULongLong));
    PK_COMPARE(v.toULongLong(), 42ULL);
    PK_COMPARE(v.toInt(), 42);
}

void VariantCase::doubleVariant()
{
    PkVariant v(3.14159);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Double));
    PK_COMPARE(v.toDouble(), 3.14159);
    PK_COMPARE(v.toInt(), 3); // truncation
    PK_COMPARE(v.toBool(), true);
    PK_VERIFY(v.canConvert<int>());
    PK_VERIFY(v.canConvert<PkString>());
}

void VariantCase::floatVariant()
{
    PkVariant v(2.718f);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Float));
    PK_COMPARE(v.toFloat(), 2.718f);
    PK_COMPARE(v.toDouble(), static_cast<double>(2.718f));
    PK_COMPARE(v.toInt(), 3); // rounding
    PK_VERIFY(v.canConvert<int>());
    PK_VERIFY(v.canConvert<double>());
    PK_VERIFY(v.canConvert<PkString>());
}

void VariantCase::boolVariant()
{
    PkVariant vt(true);
    PK_COMPARE(static_cast<int>(vt.type()), static_cast<int>(PkVariant::Bool));
    PK_VERIFY(!vt.isNull());
    PK_VERIFY(vt.isValid());
    PK_COMPARE(vt.toInt(), 1);
    PK_COMPARE(vt.toBool(), true);
    PK_COMPARE(vt.toDouble(), 1.0);
    PK_VERIFY(vt.canConvert<int>());
    PK_VERIFY(vt.canConvert<PkString>());

    PkVariant vf(false);
    PK_COMPARE(vf.toInt(), 0);
    PK_COMPARE(vf.toBool(), false);
    PK_COMPARE(vf.toDouble(), 0.0);
}

void VariantCase::stringVariant()
{
    PkVariant v("hello");
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::String));
    PK_VERIFY(!v.isNull());
    PK_VERIFY(v.isValid());
    PK_COMPARE(v.toInt(), 0);
    PK_COMPARE(v.toBool(), true);
    PK_COMPARE(v.toDouble(), 0.0);
    PK_VERIFY(v.canConvert<int>());
    PK_VERIFY(v.canConvert<PkString>());
    PK_VERIFY(v.canConvert<bool>());
    PK_VERIFY(v.canConvert<double>());

    // "123" → int 123
    PkVariant v123("123");
    PK_COMPARE(v123.toInt(), 123);
    PK_COMPARE(v123.toDouble(), 123.0);
    PK_COMPARE(v123.toBool(), true);

    // Empty string
    PkVariant ve("");
    PK_VERIFY(!ve.isNull());
    PK_VERIFY(ve.isValid());
    PK_VERIFY(ve.toString().isEmpty());
    PK_COMPARE(ve.toInt(), 0);
    PK_COMPARE(ve.toBool(), false);
    PK_COMPARE(ve.toDouble(), 0.0);
}

void VariantCase::byteArrayVariant()
{
    PkByteArray ba("binary", 6);
    PkVariant v(ba);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::ByteArray));
    PK_COMPARE(v.toByteArray().size(), 6);
    PK_VERIFY(!v.toByteArray().isEmpty());
}

void VariantCase::stringListVariant()
{
    PkStringList sl;
    sl.append(PkString("a"));
    sl.append(PkString("b"));
    sl.append(PkString("c"));
    PkVariant v(sl);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::StringList));
    PK_COMPARE(v.toStringList().size(), 3);
    PK_VERIFY(v.canConvert<PkStringList>());
}

// ── 集合类型构造与转换 ────────────────────────────────────────────────────

void VariantCase::variantListVariant()
{
    PkVariantList vl;
    vl.push_back(PkVariant(1));
    vl.push_back(PkVariant(2));
    vl.push_back(PkVariant(3));
    PkVariant v(vl);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::List));
    PK_COMPARE(static_cast<int>(v.toList().size()), 3);
    PK_VERIFY(v.canConvert<PkVariantList>());
}

void VariantCase::variantHashVariant()
{
    PkVariantHash vh;
    vh[PkString("a")] = PkVariant(1);
    vh[PkString("b")] = PkVariant(2);
    PkVariant v(vh);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Hash));
    PK_COMPARE(static_cast<int>(v.toHash().size()), 2);
    PK_VERIFY(v.canConvert<PkVariantHash>());
}

void VariantCase::variantMapVariant()
{
    PkVariantMap vm;
    vm[PkString("key")] = PkVariant(42);
    PkVariant v(vm);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Map));
    PK_COMPARE(static_cast<int>(v.toMap().size()), 1);
}

// ── 几何类型构造与转换 ────────────────────────────────────────────────────

void VariantCase::pointVariant()
{
    PkPoint pt(5, 10);
    PkVariant v(pt);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Point));
    PK_COMPARE(v.toPoint().x(), 5);
    PK_COMPARE(v.toPoint().y(), 10);
    PK_VERIFY(!v.canConvert<int>());
    PK_VERIFY(!v.canConvert<PkString>());
}

void VariantCase::pointFVariant()
{
    PkPointF pt(1.5, 2.5);
    PkVariant v(pt);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::PointF));
    PK_COMPARE(v.toPointF().x(), 1.5);
    PK_COMPARE(v.toPointF().y(), 2.5);
    PK_VERIFY(!v.canConvert<int>());
    PK_VERIFY(!v.canConvert<PkString>());
}

void VariantCase::rectVariant()
{
    PkRect r(1, 2, 3, 4);
    PkVariant v(r);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Rect));
    PK_COMPARE(v.toRect().x(), 1);
    PK_COMPARE(v.toRect().y(), 2);
    PK_COMPARE(v.toRect().width(), 3);
}

void VariantCase::rectFVariant()
{
    PkRectF rf(1.0, 2.0, 3.0, 4.0);
    PkVariant v(rf);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::RectF));
    PK_COMPARE(v.toRectF().x(), 1.0);
    PK_COMPARE(v.toRectF().width(), 3.0);
}

void VariantCase::sizeVariant()
{
    PkSize s(10, 20);
    PkVariant v(s);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Size));
    PK_COMPARE(v.toSize().width(), 10);
    PK_COMPARE(v.toSize().height(), 20);
}

void VariantCase::sizeFVariant()
{
    PkSizeF sf(10.0, 20.0);
    PkVariant v(sf);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::SizeF));
    PK_COMPARE(v.toSizeF().width(), 10.0);
    PK_COMPARE(v.toSizeF().height(), 20.0);
}

void VariantCase::lineVariant()
{
    PkLine l(0, 1, 2, 3);
    PkVariant v(l);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Line));
    PK_COMPARE(v.toLine().x1(), 0);
    PK_COMPARE(v.toLine().p2().x(), 2);
}

void VariantCase::lineFVariant()
{
    PkLineF lf(0.0, 1.0, 2.0, 3.0);
    PkVariant v(lf);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::LineF));
    PK_COMPARE(v.toLineF().x1(), 0.0);
    PK_COMPARE(v.toLineF().p2().x(), 2.0);
}

// ── 时间类型构造与转换 ────────────────────────────────────────────────────

void VariantCase::dateVariant()
{
    PkDate d(2024, 1, 15);
    PkVariant v(d);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Date));
    PK_COMPARE(v.toDate().year(), 2024);
    PK_VERIFY(v.toDate().isValid());
}

void VariantCase::timeVariant()
{
    PkTime t(12, 30, 45);
    PkVariant v(t);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Time));
    PK_COMPARE(v.toTime().hour(), 12);
    PK_VERIFY(v.toTime().isValid());
}

void VariantCase::dateTimeVariant()
{
    PkDate d(2024, 1, 15);
    PkTime t(12, 30, 45);
    PkDateTime dt(d, t);
    PkVariant v(dt);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::DateTime));
    PK_VERIFY(v.toDateTime().isValid());
}

// ── 模板方法 ──────────────────────────────────────────────────────────────

void VariantCase::fromValueAndValue()
{
    // fromValue with PkPointF
    PkPointF pt(3.0, 4.0);
    PkVariant v = PkVariant::fromValue(pt);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::PointF));
    PK_COMPARE(v.toPointF().x(), 3.0);
    PK_COMPARE(v.value<PkPointF>().x(), 3.0);
    PK_VERIFY(!v.isNull());
    PK_VERIFY(v.isValid());

    // fromValue with int
    PkVariant vi = PkVariant::fromValue(42);
    PK_COMPARE(static_cast<int>(vi.type()), static_cast<int>(PkVariant::Int));
    PK_COMPARE(vi.value<int>(), 42);

    // value<T> with wrong type (int → value<double>)
    PkVariant iv(42);
    PK_COMPARE(iv.value<int>(), 42);
    PK_COMPARE(iv.value<double>(), 42.0);
    PK_COMPARE(iv.value<bool>(), true);
}

void VariantCase::canConvert()
{
    PkVariant v(42);
    PK_VERIFY(v.canConvert<int>());
    PK_VERIFY(v.canConvert<double>());
    PK_VERIFY(v.canConvert<PkString>());
    PK_VERIFY(v.canConvert<bool>());
    PK_VERIFY(v.canConvert<float>());
    PK_VERIFY(!v.canConvert<PkPointF>());

    PkVariant ps("hello");
    PK_VERIFY(ps.canConvert<int>());
    PK_VERIFY(ps.canConvert<double>());
    PK_VERIFY(ps.canConvert<PkString>());

    PkVariant pt(PkPointF(1.0, 2.0));
    PK_VERIFY(!pt.canConvert<int>());
    PK_VERIFY(!pt.canConvert<PkString>());
    PK_VERIFY(pt.canConvert<PkPointF>());
}

void VariantCase::setValue()
{
    PkVariant v;
    v.setValue(42);
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::Int));
    PK_COMPARE(v.toInt(), 42);
    PK_VERIFY(!v.isNull());

    v.setValue(PkString("world"));
    PK_COMPARE(static_cast<int>(v.type()), static_cast<int>(PkVariant::String));
    // PkString comparison
    PkString ws = v.toString();
    PK_VERIFY(!ws.isEmpty());
}

// ── 转换边角 ──────────────────────────────────────────────────────────────

void VariantCase::conversionEdgeCases()
{
    // -5 toUInt
    PkVariant v(-5);
    PK_COMPARE(v.toUInt(), 4294967291u);
    PK_COMPARE(v.toBool(), true);

    // 999999999999LL toInt
    PkVariant v2(999999999999LL);
    PK_COMPARE(v2.toInt(), -727379969);

    // double(3.7) toInt = 4 (rounding)
    PkVariant v3(3.7);
    PK_COMPARE(v3.toInt(), 4);

    // float(3.7f) toInt = 4
    PkVariant v4(3.7f);
    PK_COMPARE(v4.toInt(), 4);

    // double(0.0) toBool = false
    PkVariant v5(0.0);
    PK_COMPARE(v5.toBool(), false);

    // double(0.5) toBool = true
    PkVariant v6(0.5);
    PK_COMPARE(v6.toBool(), true);
}

void VariantCase::nanAndInf()
{
    double nan = std::numeric_limits<double>::quiet_NaN();
    double inf = std::numeric_limits<double>::infinity();

    PkVariant vnan(nan);
    PK_VERIFY(!vnan.isNull());
    PK_VERIFY(vnan.isValid());
    PK_COMPARE(vnan.toInt(), 0);
    PK_COMPARE(vnan.toBool(), false);

    PkVariant vinf(inf);
    PK_VERIFY(!vinf.isNull());
    PK_VERIFY(vinf.isValid());
    PK_COMPARE(vinf.toInt(), 0);
    PK_COMPARE(vinf.toBool(), true);
}

void VariantCase::copyAndMove()
{
    PkVariant orig(99);
    PK_COMPARE(orig.toInt(), 99);

    // Copy
    PkVariant copy = orig;
    PK_COMPARE(copy.toInt(), 99);
    PK_COMPARE(orig.toInt(), 99); // orig unchanged

    // Move
    PkVariant moved = std::move(orig);
    PK_COMPARE(moved.toInt(), 99);
    PK_VERIFY(orig.isNull());
    PK_VERIFY(!orig.isValid());
    PK_COMPARE(orig.toInt(), 0);
}

void VariantCase::dataPointer()
{
    PkVariant v(42);
    PK_VERIFY(v.data() != nullptr);
    PK_VERIFY(v.constData() != nullptr);
    PK_COMPARE(*static_cast<const int*>(v.constData()), 42);

    PkVariant vs("hello");
    PK_VERIFY(vs.data() != nullptr);
    PK_VERIFY(vs.constData() != nullptr);

    PkVariant dv;
    PK_VERIFY(dv.data() != nullptr);
    PK_VERIFY(dv.constData() != nullptr);
}

PK_TEST_MAIN(VariantCase)