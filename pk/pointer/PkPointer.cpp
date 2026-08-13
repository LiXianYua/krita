#include "PkSharedPointer.h"
#include "PkScopedPointer.h"

// 显式实例化：给 nm -u 判据一个真的目标文件，同时强制全体成员被编译一次
// （模板成员不实例化就不编译，语法错误会藏到调用点才爆）。
namespace {
struct PkPointerInstantiationProbe {
    int v = 0;
    virtual ~PkPointerInstantiationProbe() {}
};
} // namespace

template class PkSharedPointer<PkPointerInstantiationProbe>;
template class PkWeakPointer<PkPointerInstantiationProbe>;
template class PkScopedPointer<PkPointerInstantiationProbe>;
template class PkScopedArrayPointer<PkPointerInstantiationProbe>;

extern "C" const char *pk_pointer_version() { return "pk/pointer R-04"; }
