#include "PkEventSink.h"

// 全部默认实现都是空函数体：设计选择①（virtual + 空默认实现）要求子类只
// override 关心的事件，其余事件走这里的空实现，不需要子类补一堆空函数。
//
// imageUpdated(const PkRect &) 不触碰 rect，所以不需要 PkRect 的完整定义
// ——见 PkEventSink.h 里这个方法的注释。

PkEventSink::PkEventSink() = default;

PkEventSink::~PkEventSink() = default;

void PkEventSink::aboutToAddANode(KisNode *, int) {}

void PkEventSink::nodeHasBeenAdded(KisNode *, int) {}

void PkEventSink::aboutToRemoveANode(KisNode *, int) {}

void PkEventSink::nodeHasBeenRemoved(KisNode *, int) {}

void PkEventSink::aboutToMoveNode(KisNode *, int, int) {}

void PkEventSink::nodeHasBeenMoved(KisNode *, int, int) {}

void PkEventSink::nodeChanged(KisNode *) {}

void PkEventSink::imageUpdated(const PkRect &) {}

void PkEventSink::historyStateChanged() {}
