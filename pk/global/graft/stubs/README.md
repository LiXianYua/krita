# pk/global/graft/stubs —— 当前为空

试接垫片目录。**目前没有任何垫片文件**，因为两个目标都不拉 Krita 内部头：

- 目标① TestKoIntegerMaths：只 `#include <QObject>`（pk/test/compat）、
  `<simpletest.h>`（pk/test/compat）、`"KoIntegerMaths.h"`（libs/pigment，
  自包含，只 `<cstdint>` + 宏）。无 Krita 内部头。
- 目标② driver_global_scalars.cpp：自包含，只 `#include <QtGlobal>`（pk/global/compat
  超集链）与 `<cstdio>`。

`graft_run.sh` 的 `-I` 顺序里 `$STUBS` 仍占最前一位（与 R-03 的形态一致），
将来目标变多、拉到 Krita 内部头（如 `kis_debug.h`）时，按 R-03
`pk/geometry/graft/stubs/` 的模式在同位放同名垫片即可 —— "垫片目录里有没有同名
文件"决定哪些头用真品、哪些用垫片。
