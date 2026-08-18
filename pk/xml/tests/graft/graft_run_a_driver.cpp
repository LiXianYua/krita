// pk/xml/tests/graft/graft_run_a.sh 的构建期胶水，不是对 Krita 源树的改动。
// ① 把真实、未修改的测试 .cpp 整个拉进来——PkTestBinder<T> 的显式特化必须与
//    qExec<T> 实例化点（SIMPLE_TEST_MAIN 展开出的 main()）在同一个翻译单元
//    里看见，而测试 .cpp 本身不许改一个字节去加这行 include，所以只能反过来
//    由这层不属于源树的 driver.cpp 去 include 它——形态照抄
//    pk/test/graft/graft_run.sh 的 driver.cpp。
#include "libs/image/tests/kis_dom_utils_test.cpp"
#include "binder.inc"

// ② KisTimeSpan 专属的 saveValue/loadValue 重载——真身在
//    libs/image/kis_time_span.cpp，但那个 .cpp 同一个翻译单元里还有
//    calculateIdenticalFramesRecursive() 等四个静态方法，无条件 #include
//    kis_keyframe_channel.h/kis_node.h/kis_layer_utils.h/KisStaticInitializer.h
//    ——一整棵 KisNode/KisLayerUtils 依赖树，与候选 A（kis_dom_utils.cpp）的
//    试接范围无关，也不是这次要证明的 API 形状。这两个函数体逐字照抄
//    kis_time_span.cpp:95-133（testIntegralType 断言覆盖的真实实现，不是
//    重新设计），只是挪到不拖那棵依赖树的位置——kis_time_span.h（已被
//    kis_dom_utils_test.cpp 自己 #include）只声明这两个函数、没有定义，
//    不补上会在链接期报 undefined reference。
namespace KisDomUtils {

void saveValue(QDomElement *parent, const QString &tag, const KisTimeSpan &range)
{
    QDomDocument doc = parent->ownerDocument();
    QDomElement e = doc.createElement(tag);
    parent->appendChild(e);

    e.setAttribute("type", "timerange");

    if (range.isValid()) {
        e.setAttribute("from", toString(range.start()));

        if (!range.isInfinite()) {
            e.setAttribute("to", toString(range.end()));
        }
    }
}

bool loadValue(const QDomElement &parent, const QString &tag, KisTimeSpan *range)
{
    QDomElement e;
    if (!findOnlyElement(parent, tag, &e)) return false;

    if (!Private::checkType(e, "timerange")) return false;

    int start = toInt(e.attribute("from", "-1"));
    int end = toInt(e.attribute("to", "-1"));

    if (start == -1) {
        *range = KisTimeSpan();
    } else if (end == -1) {
        *range = KisTimeSpan::infinite(start);
    } else {
        *range = KisTimeSpan::fromTimeToTime(start, end);
    }
    return true;
}

}
