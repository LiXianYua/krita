#pragma once

#include <map>
#include <vector>

#include "../string/PkString.h"
#include "../variant/PkVariant.h"
#include "PkSqlCursor.h"
#include "PkSqlError.h"

// sqlite3.h 只在 .cpp 里 include；见 PkSqlError.h/PkSqlDatabase.h 顶部同一条
// 理由（不把 <sqlite3.h> 拖进 pksql 的下游使用者）。
struct sqlite3;
struct sqlite3_stmt;

// PkSqlQuery —— QSqlQuery 的零 Qt 对应物（R-17 plan §1「QSqlQuery → PkSqlQuery」）。
//
// **单个 `sqlite3_stmt*` 的薄包装 + 内部结果集缓冲（`PkSqlCursor`，支撑任意行
// `seek`，见其头文件注释）。** 绑定在 `PkSqlDatabase` 的单一全局连接上——
// 与 `PkSqlDatabase` 同一条架构前提（R-17 plan Architecture 一节），本类不
// 持有/接受任何显式连接参数。
//
// **构造函数**：本任务（Task 2）只提供默认构造（`PkSqlQuery query;`，真实调用
// 点里最常见的形态，21+ 处）。`QSqlQuery(const QString&)` 单参构造是否需要
// "构造即执行"语义，本任务已经跑通探针钉死结论（见 R-17 plan §0 末尾订正、
// 本任务 task-2-report.md）：**不需要**——真实唯一调用点
// （`KisResourceCacheDb::addStorageType()`）对构造之后、`addBindValue`/`exec()`
// 之前的中间状态毫无依赖，探针实测"构造即执行"即便真的发生，也会因为参数
// 个数不匹配（1 个 `?` 占位符、0 个已绑定值）而失败且**没有任何副作用**（不会
// 插入多余的 NULL 行——sqlite3/Qt 对参数个数不匹配是直接拒绝执行，不是把
// 未绑定的 `?` 当 NULL 静默塞）。据此结论，这个单参构造函数**本身**（把它
// 加成"构造 = 内部 prepare(sql)，不 exec"的等价物）留给 Task 3 补——Task 2
// 的方法清单（brief 第 14-19 行）没有把它列进来，`prepare()` 已经覆盖了
// 该构造函数需要的全部能力。
class PkSqlQuery
{
public:
    // QSqlQuery::BatchExecutionMode 的对应物。真实调用点（§1 用量表：5 处
    // execBatch，1 直接 + 4 经 KisSqlQueryLoader::execBatch() 包装）**全部
    // 用默认参数调用 execBatch()，没有一处显式传 ValuesAsColumns**——本类
    // 只真正实现 ValuesAsRows 这一种语义（"第 i 行取每个绑定的第 i 个元素"，
    // R-17 plan Task 3 描述原话），ValuesAsColumns 仅保留数值占位维持类型
    // 形状完整（同 PkSqlError::UnknownError 的先例），execBatch() 传它时
    // 行为等同 ValuesAsRows（不报错，也不是本类要拦的场景）。
    enum BatchExecutionMode {
        ValuesAsRows,
        ValuesAsColumns,
    };

    PkSqlQuery();
    // QSqlQuery(const QString&) 单参构造的对应物——**只 prepare(query)，不
    // exec()**。R-17 plan §0 末尾订正（Task 2 补跑的探针）钉死的结论：这个
    // 构造函数唯一的真实调用点（`KisResourceCacheDb::addStorageType()`）在
    // 构造之后、addBindValue()/显式 exec() 之前不读取任何中间状态，"构造即
    // 执行"这个 Qt 侧的隐式尝试即便发生也因参数个数不匹配而失败、且没有可
    // 观察的副作用（不会插入多余的 NULL 行）——所以复刻成"只 prepare，不
    // exec"与复刻"prepare 后再尝试一次注定失败的 exec"，从调用点角度效果
    // 完全一致，前者更简单。构造失败（prepare 失败）不抛异常，与默认构造
    // 一样，调用方按 §1 用量表原有形态用 lastError()/isValid() 自行判断。
    explicit PkSqlQuery(const PkString &query);
    ~PkSqlQuery();

    // 不可拷贝：持有裸 `sqlite3_stmt*` 所有权，拷贝会导致双重 finalize。
    // 真实调用点全部是"作为 Private 结构体成员就地构造一次"的用法（`d->query`
    // 这种形态），不需要拷贝语义。
    PkSqlQuery(const PkSqlQuery &) = delete;
    PkSqlQuery &operator=(const PkSqlQuery &) = delete;

    // ── prepare / bind / exec ───────────────────────────────────────────
    bool prepare(const PkString &sql);
    // 具名占位符（`:name`，171 处，§0 P1 用到的形态）。name 要带前缀字符
    // （调用点写法就是 `bindValue(":resource_type", ...)`），原样传给
    // `sqlite3_bind_parameter_index`，不做任何"补冒号"之类的猜测。
    void bindValue(const PkString &name, const PkVariant &value);
    // 位置占位符 `?`（13 处 + execBatch 用）。按调用顺序对应第 1、2、3…个 `?`
    // （sqlite3 的位置参数从 1 开始编号，`addBindValue` 第 0 次调用绑定到
    // 第 1 个 `?`）。
    void addBindValue(const PkVariant &value);

    // ── execBatch 批量绑定（R-17 plan §0 P4 + Task 3 补的位置批量探针）──────
    // 具名批量：同一个占位符名字绑一个 PkVariantList，execBatch() 时"第 i 行
    // 取列表第 i 个元素"。与标量 bindValue() 是不同重载（参数类型不同，不会
    // 有二义性——PkVariant 虽然有 PkVariantList 隐式构造，但精确匹配的重载
    // 优先于经隐式转换才能匹配的重载，这里是精确匹配）。真实调用点：具名
    // 批量目前只在本类自己的单测/探针里出现，没有已知的生产调用点单独用到
    // 具名 execBatch（§1 用量表 execBatch 那 5 处全部是位置占位符），但
    // brief 明确要求"两种绑定方式都要支持"。
    void bindValue(const PkString &name, const PkVariantList &values);
    // 位置批量：`KisResourceCacheDb::deleteStorage()` 三处真实调用形态
    // （`addBindValue(QVariantList) + execBatch()`，单个 `?` 占位符）——本
    // 类按"调用顺序对应第 1、2、3…个 `?`"的规则单独存储（与标量
    // `addBindValue(PkVariant)` 各自独立编号，不是共用一份计数器）：真实
    // 调用点没有"同一条语句里既有标量位置绑定又有批量位置绑定"的形态
    // （§1 用量表 execBatch 那 5 处清一色只用批量 addBindValue），本类不
    // 处理这种混用场景。
    void addBindValue(const PkVariantList &values);

    // prepare() 之后无参数执行；支持"prepare 一次、循环 bindValue+exec"的
    // 复用模式（`KisTagResourceModel::untagResources` 的调用形态）——每次
    // `exec()` 内部会先 `sqlite3_reset` + `sqlite3_clear_bindings`，再重新
    // 应用当前 `bindValue`/`addBindValue` 存的值，不依赖"上一轮没改的占位符
    // 自动保留旧值"这种隐式状态。
    bool exec();
    // 一次性执行（PRAGMA/CREATE TABLE/`.sql` 脚本单条语句，无需先 prepare）。
    // 内部等价于 `prepare(sql)` 紧跟 `exec()`——之后仍可以 next()/value() 读
    // 结果（`KisSqlQueryLoader` 与真实调用点里 `q.exec("SELECT ...")` 后紧跟
    // `while (q.next())` 的形态需要这条）。
    bool exec(const PkString &sql);

    // 批量执行：对 prepare() 好的语句，把 bindValue(name, PkVariantList)/
    // addBindValue(PkVariantList) 存的每个列表按下标 i 取值，逐行绑定并
    // execInternal() 一次。**语义（Task 3 探针原始输出实测，见
    // task-3-report.md）：第一行失败就停止**（不继续跑剩余行，不整体
    // 回滚——没有显式事务包裹的话已成功的行保留在库里），返回值与
    // lastError()/numRowsAffected() 都取自"第一次失败"或"最后一次成功"
    // 那一次 execInternal() 调用的结果，不做跨行聚合。mode 目前只有
    // ValuesAsRows 有真实语义（见 BatchExecutionMode 注释）。
    bool execBatch(BatchExecutionMode mode = ValuesAsRows);

    // ── 游标 ─────────────────────────────────────────────────────────────
    bool next();
    bool first();
    // 任意行跳转（§0 P6，必须支持——不是"只能前进"的单向游标）。
    bool seek(int index);
    int at() const;

    // 释放当前 prepared statement 与全部内部状态（结果集/绑定值/错误/
    // isSelect 等），回到"空 query"——之后可以再 prepare() 一次全新语句。
    // 真实调用点形态：`if (!d->query.isValid()) { d->query.clear(); prepare...(); }`
    // （`KisAllResourcesModel::resetQuery()`），clear() 之后 isValid() 必须是
    // false，prepare() 必须能在其上正常重新开始。
    void clear();

    // 游标当前是否落在一条有效记录上——**不是**"query 是否曾经 prepare/exec
    // 成功过"，与 `PkSqlCursor::isValid()` 同一份判定（Qt 文档原意如此）。
    bool isValid() const;
    // 是否为 SELECT（列数 > 0，prepare 成功后即可判定，不需要等 exec()）。
    bool isSelect() const;
    // 语义上只是提示，本实现的内部游标（一次性物化整个结果集）不区分
    // forward-only/random-access，存字段、允许无操作（§1 用量表原话）。
    void setForwardOnly(bool forward);
    // 恒返回 -1（§0 P5：这台机器的 Qt SQLite 驱动完全不支持 size()，与
    // forwardOnly 无关；对应 `KisResourceMetaDataModel.cpp:56` 那处
    // `query.size() > 1` 因此恒假，是死代码，不是本类要修的行为）。
    int size() const;

    // sqlite3_last_insert_rowid()（§0 P4 [15]）。
    PkVariant lastInsertId() const;
    // sqlite3_changes()；exec() 失败或从未 exec() 过时为 -1。
    int numRowsAffected() const;

    // 当前 prepare()/exec(PkString) 的 SQL 文本。executedQuery() 与它共用
    // 同一份存储——R-17 plan §1 用量表原话："Qt 里两者绝大多数情况下相同，
    // pk/sql 可共用一份存储"。
    PkString lastQuery() const;
    PkString executedQuery() const;

    PkSqlError lastError() const;

    // QSqlQuery::boundValues() 的最小实现（R-17 全分支评审 Important #1 补齐——
    // 原实现遗漏，40 处真实调用点用途全部是 `qWarning() << q.boundValues()`
    // 诊断打印，不参与任何逻辑分支，见 plan §1 用量表「boundValues()」条目）。
    // 返回一个可遍历的绑定值集合：`m_namedBinds` 原样并入（key 已经是占位符
    // 名，形如 ":name"）；`m_positionalBinds` 按下标（"0"/"1"/...，十进制字符串）
    // 补成 key——真实调用点没有具名+位置混用的场景（§1 用量表原话），key
    // 格式冲突不是要处理的输入。语义只保证"诊断打印能看到当前已绑定的值"，
    // 不追求与 Qt 内部对位置占位符的 key 命名规则字节对齐（README §4.3 已
    // 标注为"合理选择"而非"复刻 Qt"）。
    PkVariantMap boundValues() const;

    // ── 取值 ─────────────────────────────────────────────────────────────
    // 越界/未定位/下标不存在一律返回 Invalid 的 PkVariant，不设错误
    // （§0 P2，PkSqlCursor::value() 已经是这个语义，这里直接透传）。
    PkVariant value(int col) const;
    // 具名查找：精确匹配 `sqlite3_column_name` 报出的列名（**不带表前缀**——
    // 本任务已用真实 SQL 核实：`SELECT tags.id` 这种未加 `AS` 别名的限定名
    // 写法，sqlite3_column_name 报出的是裸列名 "id"，不是 "tags.id"；
    // `KisResourceLocator.cpp:260-261` 那两处 `value("tags.id")`/
    // `value("resource_types.id")` 因此在真实 Qt 环境下也永远查不到列、返回
    // Invalid——本类原样复刻这个（真实存在的）行为，不是本类引入的偏差，
    // 详见 task-2-report.md 的探针记录）。
    PkVariant value(const PkString &name) const;

private:
    void releaseStatement();
    bool execInternal();

    sqlite3 *m_db;
    sqlite3_stmt *m_stmt;
    PkSqlCursor m_cursor;
    PkSqlError m_lastError;
    PkVariantMap m_namedBinds;
    PkVariantList m_positionalBinds;
    std::map<PkString, PkVariantList> m_namedBatchBinds;
    std::vector<PkVariantList> m_positionalBatchBinds;
    PkString m_sql;
    bool m_isSelect;
    bool m_forwardOnly;
    int m_numRowsAffected;
    PkVariant m_lastInsertId;
};
