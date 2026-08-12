// 顺序 C：**真实调用点的顺序** —— 先 <QRect>（经 compat/QRect），后 <QtGlobal>。
//
// libs/global/KisRectsGrid.h:10 `#include <QRect>` 之后
// libs/global/kis_assert.h:10  `#include <QtGlobal>` 就是这个形态，
// 任何 Krita 测试 TU（-I 里必然有 pk/test/compat）都走得到。
//
// 本 TU 守两条规则，**两条都在编译期见分晓，编过本身就是断言**：
//
//   ① compat/ 的每个类型垫片都要在包各自的 Pk 头**之前**先包 compat/QtGlobal。
//      少了那行，下面的 `#include <QRect>` 会经 ../PkRect.h 直达 PkGlobal.h，
//      由 PkGlobal.h 自己定义 qAbs；随后的 pk/test 那份垫片再定义一次 ——
//      "redefinition of 'template<class T> constexpr T qAbs(const T&)'"。
//
//   ② compat/QRect 要包 compat/QPoint 与 compat/QSize，复刻真 Qt qrect.h:44-45
//      的 qsize.h / qpoint.h。少了那两行，只 include 了 <QRect> 的调用点写
//      `QSize` 就报 "'QSize' was not declared in this scope"
//      —— libs/global/kis_global.h:315 是真实调用点。
//
// 验证过这两条守卫真的会红：拿掉 compat/QRect 的任一行 include，本 TU 当场
// 编译失败；放回去就绿。
//
// ⚠ 本 TU **不能**像另外两个 coexist TU 那样把 include 塞进匿名 namespace：
//   compat/QRect 会把 PkRect / PkRectF 的类定义一起带进去变成内部链接，
//   与 PkRect.cpp 里的 out-of-line 成员（normalized / operator| / …）对不上。
//   代价是本 TU 里的 qAbs / qRound / qFuzzy* 落在全局作用域、且来自 pk/test
//   那份垫片（与别的 TU 来自 PkGlobal.h 的同名弱符号函数体不同）——
//   所以下面**一个都不许 odr-use**，探针取值只用四则运算。

// —— 被测变量之外的东西提在最前，与另外两个 coexist TU 同一个纪律 ——
#include "coexist.h"

// ① 第一个进 TU 的 compat 垫片是**类型垫片**，不是 compat/QtGlobal。
#include "../compat/QRect"

// ② 只经上面这一行就要能写出 QPoint / QSize / QPointF / QSizeF 四个名字。
//    写成函数而不是文件作用域常量：constexpr 常量可能被完全折叠掉，
//    而我们要的是这几个名字真的解析成功。
PkCompatIncludeProbe pkCompatRectFirstProbe()
{
    const QRect r(QPoint(1, 2), QSize(3, 4));
    const QRectF rf(QPointF(1.5, 2.5), QSizeF(3.5, 4.5));

    PkCompatIncludeProbe p;
    p.rectRight = r.right();
    p.rectBottom = r.bottom();
    p.rectFRight = rf.right();
    p.rectFBottom = rf.bottom();
    return p;
}

// ③ 真实调用点里跟在 <QRect> 后面的那个 <QtGlobal>。走 pk/test 那份 ——
//    它是这条路径上唯一会与 PkGlobal.h 撞 qAbs 的东西，直接包它才是真的在测。
#include "../../test/compat/QtGlobal"
