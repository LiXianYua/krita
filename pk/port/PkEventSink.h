#pragma once

// KisNode 归 libs/image，本端口只前置声明——事件参数是裸指针，永不在这个头
// 里解引用它（裁决②：跨线程生命周期保证在同步通知里用不上，那是 Q-8/R-10
// 「投递到指定线程」的另一件事，不归本端口）。
class KisNode;
// PkRect 归 R-03（未交付），本端口只前置声明，不 include。处理方式与
// PkStream.h 里 PkByteArray 的「只声明不定义」相似但不完全相同——见下方
// imageUpdated() 的注释：两者都不需要 include 真实定义,但本类的默认实现
// 因为是空函数体、不触碰参数，所以*能*给出真实定义（不像 PkStream 那三个
// 因为要按值返回 PkByteArray 而必须留空)，这里选了「给空实现」而不是
// 「只声明不定义」，理由见 imageUpdated() 处的注释。登记见 pk/port/README.md。
class PkRect;

// PkEventSink —— 零 Qt 依赖的状态通知端口。
//
// 决定性先例：libs/image/kis_node_graph_listener.h（124 行）。Krita 自己已
// 经做过一次同款「去 QObject 化」，类注释原话："we don't want our nodes to
// be QObjects, nor to have sig-slot connections between every node and every
// mode."——PkEventSink 是这句话在端口层的延续,不是向 KisImage 的 21 个 Qt
// 信号看齐（那是信号槽形态，是给 Qt Interview Model 用的异步通知，不是本端
// 口要复刻的同步语义）。
//
// 照抄 KisNodeGraphListener 的三个设计选择：
//   1. virtual + 空默认实现，不用 = 0——子类只 override 关心的事件，不必为
//      不关心的事件写空函数体。
//   2. 成对命名：aboutToXxx / xxxHasBeenDone。只有天然带「之前/之后」两阶段
//      的操作（增/删/移动节点）才用这个命名；没有对应「之前」阶段的通知
//      （nodeChanged、imageUpdated、historyStateChanged）保持单个事件，不
//      硬凑一对——这也是 KisNodeGraphListener 自己的做法（它的 nodeChanged()/
//      nodeCollapsedChanged() 同样是单个事件）。
//   3. 接口自身可以带状态（KisNodeGraphListener 有非虚的 graphSequenceNumber()，
//      状态在 pimpl 里）。本类目前没有对应需求——材料里没有导出等价的状态
//      诉求，按「范围上界=实测」不预先发明；哪个批次真的需要序列号语义，
//      再按需加。
//
// 消费者是防腐层（转发到 Flutter）与测试记录器（本任务 tests/test_eventsink.cpp
// 的记录型子类）。R-12 只出接口，不出实现——没有产品级子类，也没有 Krita
// 侧的调用点接进来（那是 S 批次的事）。
//
// 事件按三类分组，每个事件都标了来源：
//   ① 图层树变更（对应 KisNodeGraphListener 的节点增删移改）
//   ② 区域重绘完成（对应 KisImage::sigImageUpdated）
//   ③ 撤销栈变更（对应 KisUndoStore::historyStateChanged——这是本端口最大
//     的收益点：KisUndoStore 的 6 个纯虚方法本来就是 100% 纯 C++ 接口，唯独
//     因为这一个信号被迫继承 QObject + Q_OBJECT；通知面一挪到这里，
//     KisUndoStore 的端口版本就能彻底摆脱 QObject。）
class PkEventSink
{
public:
    PkEventSink();
    virtual ~PkEventSink();

    // ── ① 图层树变更 ──────────────────────────────────────────────────
    // 全部 6 个方法来源：libs/image/kis_node_graph_listener.h 的对应同名方法
    // （行号见各方法注释）。同时对照 libs/image/kis_image_signal_router.h
    // 的 3 个 emitXxx() 方法确认这是真实调用面：emitNodeHasBeenAdded()、
    // emitAboutToRemoveANode()、emitNodeChanged()——router 的另外 4 个
    // emitXxx()（emitNotification/emitNotifications 系列里除
    // LayersChangedSignal 外的图像属性变更调度、emitRequestLodPlanes-
    // SyncBlocked、emitNotifyBatchUpdateStarted/Ended）不在这三类分组之内
    // （前者是 KisImage 尺寸/色彩空间/分辨率变更的调度器，后两者是画布 LOD/
    // 批量重绘调度，都不是「图层树变更」），本任务不做。
    //
    // 评审 I-4：原先这里写"另外 5 个 emitXxx 不做"，第 5 个是
    // `emitNotification(LayersChangedSignal)`——`KisImage::notifyLayersChanged()`
    // （kis_image.cc:1793）就是走这条路径，由
    // `libs/image/commands/kis_image_change_layers_command.cpp:28,38` 在
    // redo/undo 整棵图层栈替换（拼合、合并全部）时发出，不伴随任何 per-node
    // 增删事件。消费者 `kis_dummies_facade_base.cpp:99` 正是维护图层树镜像
    // 那一层——少这一个，图层树镜像在拼合/撤销后会静默过期。现在已经用
    // `layersChanged()` 纳入，剩下真正不做的是另外 4 个。

    // 来源：kis_node_graph_listener.h:45 aboutToAddANode()。
    virtual void aboutToAddANode(KisNode *parent, int index);

    // 来源：kis_node_graph_listener.h:50 nodeHasBeenAdded() +
    // kis_image_signal_router.h:94 emitNodeHasBeenAdded()（确认真实调用面）。
    //
    // 评审 I-3：原型上的 KisNodeAdditionFlags 参数（libs/image/
    // KisNodeAdditionFlags.h:13）**只有一个位**——DontActivateNode。消费点
    // kis_dummies_facade_base.cpp:159 `if (flags.testFlag(DontActivateNode))
    // return;`——语义是"新增图层后不要把它设为当前图层"，防腐层要转发给
    // Flutter 的恰恰是这一位，不能整族裁掉。只有一个位不需要 QFlags 归口，
    // 一个 bool 就能带上语义，比再造一个 Pk 版本的 flags 类型便宜得多。
    virtual void nodeHasBeenAdded(KisNode *parent, int index, bool dontActivateNode = false);

    // 来源：kis_node_graph_listener.h:55 aboutToRemoveANode() +
    // kis_image_signal_router.h:109 emitAboutToRemoveANode()。
    virtual void aboutToRemoveANode(KisNode *parent, int index);

    // 来源：kis_node_graph_listener.h:60 nodeHasBeenRemoved()。
    virtual void nodeHasBeenRemoved(KisNode *parent, int index);

    // 来源：kis_node_graph_listener.h:66 aboutToMoveNode()。移动 = 先移除
    // 后添加同一个节点，但走的是独立的一对事件，不是拆成两次增删事件。
    virtual void aboutToMoveNode(KisNode *node, int oldIndex, int newIndex);

    // 来源：kis_node_graph_listener.h:72 nodeHasBeenMoved()。
    virtual void nodeHasBeenMoved(KisNode *node, int oldIndex, int newIndex);

    // 来源：kis_node_graph_listener.h:74 nodeChanged() +
    // kis_image_signal_router.h:29/89 emitNodeChanged()/sigNodeChanged(KisNodeSP)
    // ——节点参数按裁决②从 KisNodeSP 降成裸 KisNode*。
    virtual void nodeChanged(KisNode *node);

    // 来源：KisImage::notifyLayersChanged()（kis_image.cc:1793，走
    // signalRouter.emitNotification(LayersChangedSignal)）+
    // kis_image_change_layers_command.cpp:28,38——redo/undo 整棵图层栈替换
    // （拼合、合并全部）时发出，不伴随任何 per-node 增删事件，因此是单个
    // 事件而不是 aboutToXxx/xxxHasBeenDone 那种成对事件。消费者
    // kis_dummies_facade_base.cpp:99 维护图层树镜像，评审 I-4 指出漏了这个
    // 会导致镜像在拼合/撤销后静默过期。
    virtual void layersChanged();

    // ── ② 区域重绘完成 ────────────────────────────────────────────────
    // 来源：libs/image/kis_image.h:818 sigImageUpdated(const QRect &)，原注
    // 释："Emitted whenever an action has caused the image to be
    // recomposited. Parameter is the rect that has been recomposited."
    //
    // QRect 参数用 PkRect 前置声明代替（PkRect 归 R-03，未交付）。这个方法
    // 的默认实现是空函数体、不触碰 rect，因此**不需要 PkRect 的完整定义就能
    // 编译并链接**——和 PkStream.h 里 readAll()/peek()/readLine() 三个「只
    // 声明不定义」的方法不是同一回事：那三个是按值*返回* PkByteArray，返回
    // 一个不完整类型的值连空函数体也写不出来（`return PkByteArray();` 需要
    // 完整类型才能构造），只能先把符号形状钉下来、留给链接期报错。这里的
    // 参数是 `const PkRect &`，只传引用不构造不拷贝，空函数体天然合法，于是
    // 直接给了真实的空实现，维持「virtual + 空默认实现」在全部事件上的一致
    // 性（否则这一个事件的默认版本会在任何没有 override 它的子类身上造成
    // vtable 未决议符号——只声明不定义对虚函数不安全，非虚函数才行）。
    //
    // 代价：在 PkRect 交付前，没有办法从外部构造一个 PkRect 实参去调用这个
    // 方法，所以 tests/test_eventsink.cpp 里验证不了它的调用链路，只验证得
    // 了「默认实现存在、类可以被实例化」。见 pk/port/README.md 的登记表。
    virtual void imageUpdated(const PkRect &rect);

    // ── ③ 撤销栈变更 ──────────────────────────────────────────────────
    // 来源：libs/command/kis_undo_store.h:64 historyStateChanged()。
    //
    // 这是把通知面从 KisUndoStore 挪出去的收益所在：KisUndoStore 除了这一个
    // 信号之外的 6 个方法全是纯虚（presentCommand/undoLastCommand/addCommand/
    // beginMacro/endMacro/purgeRedoState），本来就不需要 QObject；唯独这一
    // 个 Q_SIGNALS 段里的 historyStateChanged() 逼着它继承 QObject + Q_OBJECT。
    // 通知面一挪到 PkEventSink，KisUndoStore 的端口版本就能变成 100% 纯
    // C++ 接口，不再需要 QObject。
    virtual void historyStateChanged();
};
