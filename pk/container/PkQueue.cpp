#include "PkQueue.h"

#include <string>

// 理由与 PkStack.cpp 逐字相同（见那里，含「为什么只点自己这一层」）。
template class PkQueue<int>;
template class PkQueue<std::string>;
