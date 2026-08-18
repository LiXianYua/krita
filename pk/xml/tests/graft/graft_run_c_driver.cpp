// R-25 Task 1 判据②候选 C（`libs/psd/psd_layer_section.cpp:596` 的降级试探）：
//
// `-fsyntax-only` 直接对 psd_layer_section.cpp 本身试接（见 graft_run_c.sh
// 里贴的原始失败记录）在文件顶部第一个 include（`psd_layer_section.h` →
// `kritapsd_export.h`）就 fatal error——往下还要连续解析
// `KoColor.h`/`KoColorSpace.h`/`kis_image.h`/`kis_node.h`/`KoShapeManager.h`
// 等一整条 libs/pigment + libs/image + libs/flake 的生产依赖链，每一层都缺
// CMake `generate_export_header()` 才产出的导出头——这不是"多补几个 stub
// 文件"能在时间盒内解决的规模（`plugins/impex/libkra/kis_kra_loader.cpp`
// 候选也在同一类墙上撞停，见计划 Task 1 checklist 与 R-25 task-1-report.md
// 的候选 A/B 记录），且这些库整体不在本任务 `pk/xml` 的 `locks` 范围内。
//
// 退化到计划里已有先例的处理方式（README §11.3 / 本计划设计①-b 对
// `SvgParser.cpp:201` 的处理："没有找到干净的真实测试类候选，改用形状对齐的
// 内部测试自证 API 正确"）：不编译真实文件，改为在这里**逐字符**复刻
// `psd_layer_section.cpp:590-600` 里 `mergePatternsXMLSection()` 函数用到
// `importNode()` 的那几行调用形状（类型、参数顺序、参数类型全部对齐），只用
// pk/xml 自己已交付的 compat 垫片编译——只证明"签名字面兼容，编译器不报
// no matching function"，不证明整份 psd_layer_section.cpp 能编译。
//
// 原文（未改一个字，逐字抄自 libs/psd/psd_layer_section.cpp:578-600）：
//
//   void mergePatternsXMLSection(const QDomDocument &src, QDomDocument &dst)
//   {
//       QDomNode srcPatternsNode = findNodeByKey(ResourceType::Patterns, src.documentElement());
//       QDomNode dstPatternsNode = findNodeByKey(ResourceType::Patterns, dst.documentElement());
//
//       if (srcPatternsNode.isNull())
//           return;
//       if (dstPatternsNode.isNull()) {
//           dst = src;
//           return;
//       }
//
//       KIS_ASSERT_RECOVER_RETURN(!srcPatternsNode.isNull());
//       KIS_ASSERT_RECOVER_RETURN(!dstPatternsNode.isNull());
//
//       QDomNode node = srcPatternsNode.firstChild();
//       while (!node.isNull()) {
//           QDomNode importedNode = dst.importNode(node, true);
//           KIS_ASSERT_RECOVER_RETURN(!importedNode.isNull());
//
//           dstPatternsNode.appendChild(importedNode);
//           node = node.nextSibling();
//       }
//   }
//
// 下面的 `mergePatternsXMLSectionShape()` 是这段代码的形状对齐版本——去掉了
// `findNodeByKey()`（KisDomUtils 里的另一个函数，不是本任务要验证的对象）与
// `KIS_ASSERT_RECOVER_RETURN`（Krita 生产宏，同样不是本任务要验证的对象），
// 其余每一行 `QDomNode`/`QDomDocument`/`.importNode(`/`.appendChild(`/
// `.firstChild()`/`.nextSibling()`/`.isNull()` 的类型与调用形状逐字保留。

#include <QDomDocument>
#include <QDomNode>
#include <QString>

#include <cassert>
#include <cstdio>

void mergePatternsXMLSectionShape(const QDomDocument &src, QDomDocument &dst)
{
    QDomNode srcPatternsNode = src.documentElement();
    QDomNode dstPatternsNode = dst.documentElement();

    if (srcPatternsNode.isNull())
        return;
    if (dstPatternsNode.isNull()) {
        dst = src;
        return;
    }

    QDomNode node = srcPatternsNode.firstChild();
    while (!node.isNull()) {
        // 与真实调用点 psd_layer_section.cpp:596 字面相同的调用：
        //   QDomNode importedNode = dst.importNode(node, true);
        QDomNode importedNode = dst.importNode(node, true);
        assert(!importedNode.isNull());

        dstPatternsNode.appendChild(importedNode);
        node = node.nextSibling();
    }
}

int main()
{
    // 简化掉真实调用点里的 findNodeByKey()（按属性值找子节点，是
    // KisDomUtils 的另一个函数，不是本试探要验证的对象）——测试数据直接把
    // 文档根设成 "Patterns"，这样 `src.documentElement()` 就是真实代码里
    // `findNodeByKey(...)` 本该找到的那个节点，其余调用形状不受影响。
    QDomDocument src;
    src.setContent(QString("<Patterns><a x=\"1\"><b>t</b></a><c/></Patterns>"));

    QDomDocument dst;
    dst.setContent(QString("<Patterns/>"));

    mergePatternsXMLSectionShape(src, dst);

    const QString out = dst.toString(-1);
    const bool ok = out.contains(QString("<a x=\"1\">")) && out.contains(QString("<c/>"));
    std::printf(ok ? "shape-call OK: %s\n" : "shape-call MISMATCH: %s\n", out.PkToUtf8().c_str());
    return ok ? 0 : 1;
}
