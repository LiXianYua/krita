/*
 *  SPDX-License-Identifier: GPL-3.0-or-later
 */

#include <simpletest.h>
#include "TestFallBackColorTransformation.h"

#include "KoColorTransformation.h"
#include <KoFallBackColorTransformation.h>
#include <KoColorSpaceRegistry.h>


struct KoDummyColorTransformation : public KoColorTransformation
{
    KoDummyColorTransformation()
    {
      m_parameters << 1 << 2;
    }
    PkList<PkVariant> m_parameters;
    void transform(const quint8 */*src*/, quint8 */*dst*/, qint32 /*nPixels*/) const override
    {
    }
    PkList<PkString> parameters() const override
    {
      PkList<PkString> s;
      s << "test";
      return s;
    }
    int parameterId(const PkString& name) const override
    {
      if(name == "test")
      {
        return 1;
      } else {
        return 0;
      }
    }
    void setParameter(int id, const PkVariant& parameter) override
    {
      m_parameters[id] = parameter;
    }
};

void TestFallBackColorTransformation::parametersForward()
{
  KoDummyColorTransformation* dummy = new KoDummyColorTransformation;
  KoFallBackColorTransformation* fallback = new KoFallBackColorTransformation(KoColorSpaceRegistry::instance()->rgb8(),
                                                                              KoColorSpaceRegistry::instance()->rgb16(),
                                                                              dummy);
  PK_COMPARE(fallback->parameters().size(), 1);
  // PK_COMPARE 的形参按 const auto& 绑定到独立语句；`fallback->parameters()[0]`
  // 是「函数返回的临时 PkList 的 operator[] 引用」，C++ 不会把临时容器寿命延长到
  // 那条声明语句之外——绑定语句结束后临时列表析构，pkCompareActual_ 悬垂。
  // 先落到具名局部再下标，断言语义与原文（比较宏直接传实参、临时活在整条
  // 调用语句内）完全一致，只是绕开 PkTest 宏对「临时容器[下标]」的生命周期陷阱。
  const PkList<PkString> params = fallback->parameters();
  PK_COMPARE(params[0], PkString("test"));
  PK_COMPARE(fallback->parameterId("test"), 1);
  PK_COMPARE(fallback->parameterId("other"), 0);
  fallback->setParameter(0, -1);
  fallback->setParameter(1, "value");
  PK_COMPARE(dummy->m_parameters[0], PkVariant(-1));
  PK_COMPARE(dummy->m_parameters[1], PkVariant("value"));
  delete fallback;
}

PK_TEST_GUILESS_MAIN(TestFallBackColorTransformation)
