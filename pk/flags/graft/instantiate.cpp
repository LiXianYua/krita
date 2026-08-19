// 这不是 libs/image 里的真实测试文件，是**复刻调用点形状的 driver**——真实压
// QFlags 的测试类（TestCompositeOpInversion.cpp 等）在 libs/pigment 等大模块，
// 编它们需要该模块自己的 CMake 导出头 + QTransform/QColor/QMetaType 等几十个未交付
// 类型（依赖墙），本任务 locks=pk/flags 物理编不过。见 R线-spec「依赖墙挡住真实
// 测试类时」的降级四条件。校验值来自真 Qt 探针（R-20 plan「问 0」）。
#include "libs/image/KisNodeAdditionFlags.h"   // 真实生产头，零改动

int main() {
    KisNodeAdditionFlags f;                       // 默认构造 = 0
    if (int(f) != 0) return 1;                     // operator Int() on zero

    f |= KisNodeAdditionFlag::DontActivateNode;   // operator|=(Enum)
    if (int(f) != 1) return 2;
    if (!f.testFlag(KisNodeAdditionFlag::DontActivateNode)) return 3;  // testFlag on active bit

    f.setFlag(KisNodeAdditionFlag::DontActivateNode, false);            // setFlag off
    if (int(f) != 0) return 4;                     // after clearing, value is 0

    // testFlag(None) on zero: 探针确认 testFlag(0) on zero 返回 true
    if (!f.testFlag(KisNodeAdditionFlag::None)) return 5;

    // enum|flags 自由 operator|
    KisNodeAdditionFlags g = KisNodeAdditionFlag::None | f;
    if (int(g) != 0) return 6;

    // operator&(Enum)
    f |= KisNodeAdditionFlag::DontActivateNode;
    KisNodeAdditionFlags h = f & KisNodeAdditionFlag::DontActivateNode;
    if (int(h) != 1) return 7;

    return 0;
}