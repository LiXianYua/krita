// 真 Qt 5.15 的调试流输出探针 —— 只用于给 pk/geometry/PkGeometryDebug.cpp 的
// 期望值取证。**本目录（pk/*/oracle/）按 S线-spec 判据② 排除，允许写 Q* 名。**
//
// 复现命令（macOS，2026-09-11 实测）：
//   P=/Users/liyang/Developer/projects/krita-ci-env/_install
//   /usr/bin/c++ -std=c++17 -fPIC -F$P/lib \
//       -I$P/lib/QtCore.framework/Headers -I$P/lib/QtGui.framework/Headers \
//       pk/geometry/oracle/debugstream_qt.cpp \
//       -framework QtCore -framework QtGui -Wl,-rpath,$P/lib -o /tmp/dbgstream_qt
//   QT_QPA_PLATFORM=offscreen /tmp/dbgstream_qt 2>&1
#include <QDebug>
#include <QPoint>
#include <QPointF>
#include <QRect>
#include <QRectF>
#include <QLine>
#include <QLineF>
#include <QSize>
#include <QSizeF>
#include <QMargins>
#include <QMarginsF>
#include <QPolygon>
#include <QPolygonF>
#include <QTransform>

int main()
{
    qDebug() << "point"      << QPoint(1, 2);
    qDebug() << "pointf"     << QPointF(1, 2);
    qDebug() << "pointf_neg" << QPointF(-1.5, 2.25);
    qDebug() << "size"       << QSize(1, 2);
    qDebug() << "sizef"      << QSizeF(1, 2);
    qDebug() << "rect"       << QRect(1, 2, 3, 4);
    qDebug() << "rectf"      << QRectF(1, 2, 3, 4);
    qDebug() << "line"       << QLine(1, 2, 3, 4);
    qDebug() << "linef"      << QLineF(1, 2, 3, 4);
    qDebug() << "margins"    << QMargins(1, 2, 3, 4);
    qDebug() << "marginsf"   << QMarginsF(1, 2, 3, 4);

    QPolygon poly;
    poly << QPoint(1, 2) << QPoint(3, 4) << QPoint(5, 6);
    qDebug() << "polygon"    << poly;
    qDebug() << "polygon_empty" << QPolygon();

    QPolygonF polyf;
    polyf << QPointF(1, 2) << QPointF(3, 4) << QPointF(5, 6);
    qDebug() << "polygonf"   << polyf;
    qDebug() << "polygonf_empty" << QPolygonF();

    // 链式与 nospace 交互：这两个是「整串一次插入」写法的判据
    qDebug() << "chain" << QPointF(1, 2) << QLineF(1, 2, 3, 4) << QRectF(1, 2, 3, 4);
    qDebug() << "chain_text" << "a" << QPointF(1, 2) << "b";
    qDebug().nospace() << "nospace" << QPoint(0, 0) << QPointF(0, 0);

    // ---- QTransform 格式族（S-16 修复轮 2 新增）----
    // 八种档位各一例，覆盖 type() 惰性重算的三条构造起点：默认 (TxNone,TxNone)、
    // 六参 (TxNone,TxShear)、九参 (TxNone,TxProject)。`project` 用九参但九个分量
    // 构成的是纯平移 —— 它验证 type() 重算后**如实报 TxTranslate**、不因为构造时
    // 的 m_dirty=TxProject 就谎报 TxProject。
    qDebug() << "identity"  << QTransform();
    qDebug() << "six"       << QTransform(1, 2, 3, 4, 5, 6);
    qDebug() << "nine"      << QTransform(1.5, 2, 3, 4, 5, 6, 7, 8, 9);
    qDebug() << "translate" << QTransform().translate(3, 4);
    qDebug() << "scale"     << QTransform().scale(2, 3);
    qDebug() << "rotate"    << QTransform().rotate(90);
    qDebug() << "project"   << QTransform(1, 0, 0, 0, 1, 0, 0.1, 0.2, 1);
    qDebug() << "neg"       << QTransform().scale(-1.25, 1);
    return 0;
}
