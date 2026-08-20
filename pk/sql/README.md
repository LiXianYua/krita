# pk/sql —— R-17（`Qt::Sql` → sqlite3 C API 换库）

本目录交付 `QSqlDatabase`/`QSqlQuery`/`QSqlError` 的零 Qt 替代
（`PkSqlDatabase`/`PkSqlQuery`/`PkSqlError`），底层直接调用 `sqlite3` C API
（决策 D-7：`docs/迁移执行计划.md` 第 64/500 行——"资源系统保留 SQLite，从
`Qt::Sql` 换成 `sqlite3` C API"，"改动限于把 `QSqlQuery` 换成
`sqlite3_stmt`"）。

**分类（线级 spec「两类任务」判据）：乙类 · 换库**。`Qt::Sql` 是对 `sqlite3`
的一层 API 包装，不是有确定输入→输出映射的值类型——真正需要对齐的是
`QSqlDatabase`/`QSqlQuery`/`QSqlError` 各方法的返回值/错误分类语义（被 Qt 的
SQLite 驱动实现钉死，不是 SQL 本身的语义），必须探针实测，下面 §0 是这些探针
的原始输出。

本文件从线级 plan `docs/superpowers/plans/R-17.md` **完整搬运**§0/§1/§2 三节
（plan 文件不在 fork 仓库里，S 线消费者的 worktree 读不到它，本文件是唯一能
读到的副本——照抄 `pk/xml/README.md` 的先例），并补上 Task 3 的试接/driver
结论、Task 4 的产物终验与剥离笔记。

分四个 Task 交付：Task 1（CMake 骨架 + `PkSqlError` + `PkSqlDatabase`，
commit `8cfaebe`）、Task 2（`PkSqlQuery` 核心，commit `11c92b1`）、Task 3
（`execBatch` + 真实调用点试接，commit `dd59376`）、Task 4（本文件 + 判据③
终验，本 commit）——本 plan 上限 5 个 Task，实际 4 个即交付完毕，无 Task 5。

---

## 0. 探针实测（判据 0，逐条原始输出）

**环境**：`/mnt/ssd-disk/liyang/projects/krita-ci-env/_install`（Qt 5.15.7，含
`libQt5Sql.so.5.15.7` + `QSQLITE` 驱动插件）。**注意（2026-08-18 订正）**：
`krita/AGENTS.md` 与线级 spec 里写的
`source /mnt/ssd-disk/liyang/projects/krita-ci-env/env` 指向的是 `liyang` 的
私有副本（权限 `600`，属主 `liyang`，本项目执行账号 `qiansenwei` 读不到，会
直接 `permission denied`）。这台机器上 `qiansenwei` 自己有一份内容与用途相同
的副本，只是挂载位置不同：

```
source /home/qiansenwei/workspace/Krita_Linux_liyang/krita-ci-env/env
```

跑测试/配置构建时一律用这条路径替换文档里写的 `/mnt/ssd-disk/liyang/...` 那条
（已实测 `test -r` 可读）。本任务 §0 的探针是在权限问题发现之前跑的，当时绕过
方式是**手动设置 `-I`/`-L`/`LD_LIBRARY_PATH`（不 `source` 任何一份 `env`）**，
下面这条命令依然有效（`_install` 下的头文件/库文件本身世界可读），两种方式
任选：

```bash
QT=/mnt/ssd-disk/liyang/projects/krita-ci-env/_install
g++ -fPIC -std=c++17 probe.cpp -o probe \
  -I$QT/include -I$QT/include/QtCore -I$QT/include/QtSql \
  -L$QT/lib -lQt5Sql -lQt5Core -Wl,-rpath-link,$QT/lib
LD_LIBRARY_PATH=$QT/lib ./probe
```

### P1：基础错误分类——`type()` 按"Qt 驱动内部哪个 sqlite3 调用失败"分，不按 SQL 语义分

探针（节选，完整源码见 `KisResourceCacheDb.cpp` 等的调用形态，§1 用量表）：
```cpp
QSqlDatabase db = QSqlDatabase::addDatabase("QSQLITE");
db.setDatabaseName(":memory:");
db.open();
QSqlQuery q0; q0.exec("CREATE TABLE t (id INTEGER PRIMARY KEY, name TEXT NOT NULL UNIQUE)");
// [2] 语法错误
QSqlQuery q; q.exec("SELCT * FROM t");
// [3] 表不存在
QSqlQuery q; q.exec("SELECT * FROM nonexistent_table");
// [4] UNIQUE 冲突 / [5] PRIMARY KEY 冲突 / [6] NOT NULL 冲突（bindValue 绑 QVariant() null）
// [7] prepare 期语法错误
```

原始输出：
```
[2] 语法错误 exec ok=0
  type()=2 (StatementError)  isValid()=true
  text()=[near "SELCT": syntax error Unable to execute statement]
  nativeErrorCode()=[1]  databaseText()=[near "SELCT": syntax error]
  driverText()=[Unable to execute statement]

[3] 表不存在 exec ok=0
  type()=2 (StatementError)  isValid()=true
  text()=[no such table: nonexistent_table Unable to execute statement]
  nativeErrorCode()=[1]  databaseText()=[no such table: nonexistent_table]
  driverText()=[Unable to execute statement]

[4] UNIQUE 冲突 exec ok=0
  type()=1 (ConnectionError)  ← 不是 StatementError！  isValid()=true
  text()=[UNIQUE constraint failed: t.name Unable to fetch row]
  nativeErrorCode()=[19]  databaseText()=[UNIQUE constraint failed: t.name]
  driverText()=[Unable to fetch row]

[5] PRIMARY KEY 冲突 exec ok=0
  type()=1 (ConnectionError)  isValid()=true
  text()=[UNIQUE constraint failed: t.id Unable to fetch row]
  nativeErrorCode()=[19]

[6] NOT NULL 冲突（bindValue 绑 QVariant() null）exec ok=0
  type()=1 (ConnectionError)  isValid()=true
  text()=[NOT NULL constraint failed: t.name Unable to fetch row]
  nativeErrorCode()=[19]

[7] prepare 期语法错误 prepOk=0
  type()=2 (StatementError)  isValid()=true
  text()=[near "VALUES": syntax error Unable to execute statement]
  nativeErrorCode()=[1]
```

**结论（对齐要求，全部判定"必须对齐"）**：
- `text()` = `databaseText()` + （`driverText()` 非空则加一个空格再拼上）—— 不是分号/冒号拼接
- `nativeErrorCode()` = sqlite3 原生 result code 的十进制字符串（`1`=`SQLITE_ERROR`、
  `19`=`SQLITE_CONSTRAINT`），语法错误与"表不存在"都归 `SQLITE_ERROR`（sqlite3 层面
  这两者本就同一个 result code，不需要 `pk/sql` 额外区分）
- `driverText()` 是 Qt 驱动**按内部调用点**给的固定短语（不是从 sqlite 拿到的），
  已知取值：`"Unable to execute statement"`（`sqlite3_prepare_v2`/`sqlite3_step` 失败，
  且失败发生在**第一次 `step`**）、`"Unable to fetch row"`（约束冲突发生在
  `sqlite3_step` 返回 `SQLITE_CONSTRAINT` 时，Qt 驱动的实现把"`step` 拿不到行"的
  这条路径统一归 `Unable to fetch row`）、`"Unable to begin/commit/rollback
  transaction"`（P3）
- **`type()` 分类表**（`pk/sql` 必须原样复刻，不能按"看起来更合理"的分类改）：
  `SQLITE_ERROR`（语法错误/表不存在，`prepare` 阶段失败）→ `StatementError`；
  `SQLITE_CONSTRAINT`（`step` 阶段失败，即约束冲突）→ **`ConnectionError`**；
  事务操作失败 → `TransactionError`（P3）

### P2：`next()`/`value()` 越界与未定位行为

```
[8] next() sequence: first=1 second(耗尽)=0 third(耗尽后再调)=0
    lastError after 耗尽 next()：type=0(NoError) isValid=false（不是错误，是正常终止）

[9] value() 当 next() 返回 false（空结果集）:
    next()=0  value(0).isValid()=0  value(0).isNull()=1
    value(0).toString()=[]  value(0).toInt()=0
    （stderr 侧带一条 Qt 内部 qWarning："QSqlQuery::value: not positioned on a valid record"，
    不进 QSqlError，是纯日志噪音）
    lastError 仍是 NoError（未定位取值不算 SQL 错误）

[9b] value() 在 next() 从未被调用过时：同 [9]，value(0).isValid()=0

[10] value(99)（列下标越界）: isValid()=0  toString()=[]
```

**结论**：未定位/越界取值一律返回**空/invalid 的 `PkVariant`**，不设错误、不抛异常，
`lastError()` 保持 `NoError`——`pk/sql` 的 `value()` 对这三种情形（未 `next()`、
`next()` 已耗尽、下标越界）返回值语义完全一致，不需要分情况处理。

### P3：事务

```
[12] db.transaction()=1
     插入一行, db.commit()=1
     lastError after commit：NoError

     再 db.transaction()（新事务）, 插入一个会违反 UNIQUE 的行, exec()=0（失败，事务未自动回滚）
     db.rollback()=1

[13] 嵌套 transaction()（已有一个进行中的事务时再调一次）:
     first=1  second(嵌套)=0
     lastError: type()=3 (TransactionError)  isValid=true
     text()=[cannot start a transaction within a transaction Unable to begin transaction]
     nativeErrorCode()=[]（空——这是 Qt 驱动自己拦的，没有真的调 sqlite3，所以没有
       原生错误码）
```

**结论**：`transaction()` 不支持嵌套（Qt SQLite 驱动自己用一个内部布尔标志拦
第二次 `BEGIN`，不是 sqlite3 报错）——`nativeErrorCode()` 在这种情况下必须是空串，
不能塞一个假的 sqlite 错误码。`KisDatabaseTransactionLockAdapter`（见附录源码）
本身用一个 `m_transactionStarted` 布尔保证不会嵌套调用，`pk/sql` 只需在
`PkSqlDatabase::transaction()` 里做同款的"已有进行中事务再调直接失败"防御即可，
不需要处理"两个不同 `PkSqlDatabase` 实例互相嵌套"这种本任务范围外的场景（只有
一个全局连接，§1「连接模型」）。

### P4：`PRAGMA`（不可参数化）+ `lastInsertId()` + 具名批量 `execBatch`

```
[14] PRAGMA exec ok=1  lastError: NoError
[15] lastInsertId() = 101（AUTOINCREMENT，取的是插入后 sqlite3_last_insert_rowid()）
[16] execBatch（具名占位符 + QVariantList，ValuesAsRows 模式）ok=1  lastError: NoError
```

`KisResourceCacheDb.cpp` 实测有 `QString("PRAGMA foreign_keys = %1").arg(...)` 这种
字符串拼接（§1「不可参数化」），探针确认 `PRAGMA` 语句本身对 `exec(QString)` 一次性
执行完全正常，不需要 `pk/sql` 做任何特殊处理——只是 `PkSqlQuery::exec(PkString)`
（无 `prepare`/`bindValue` 的一次性执行路径）本来就要支持。

### P5：`size()` 恒为 `-1`（不区分 forward-only）

```
[A] forwardOnly=true 时 size() = -1
[B] forwardOnly=false（默认）时 size() = -1
```

**这台机器的 Qt SQLite 驱动完全不支持 `size()`，恒返回 `-1`，与是否 `forwardOnly`
无关。** 对照 §1 用量表：`KisResourceMetaDataModel.cpp:56` 唯一一处 `query.size() > 1`
的重复项警告因此**必然恒假、是死代码**——`pk/sql` 的 `size()` 直接恒返回 `-1`
即可对齐，Task 2 单测钉住这一行为，Task 3 试接/驱动记录里标注这处调用点是
已确认死代码（不算范围蔓延，也不是遗漏）。

### P6：`seek()` 随机访问（支撑 Model 类的"活游标"用法）

```
[C] forwardOnly=true，next() 过一次后 seek(0) = true（返回当前行成功，不是"禁止"）
[D] 非 forwardOnly，next() 两次后 seek(0)（往回跳）= true，value(0) 拿到第 0 行数据 = 1
```

**Qt 的 SQLite 驱动对非 forward-only 查询支持任意行 `seek`**（内部按需要把已见过的
行缓冲住），这正是 `KisAllResourcesModel`/`KisResourceTypeModel`/`KisStorageModel`/
`KisTagModel`/`KisAllTagResourceModel` 把 `data(index, role)` 直接接到
`d->query.seek(index.row())` 的前提（§1「架构」）。`pk/sql` 的 `PkSqlQuery::seek(int)`
**必须支持任意行跳转，不能只做 forward-only 迭代**——这是本任务比"照抄一个
`sqlite3_stmt` 单向游标"重得多的一条要求。

**落地**（Task 2 实现，见下 §2）：`PkSqlCursor` 在 `exec()` 时一次性把整个结果集
物化进 `std::vector<PkVariantList>`（`sqlite3_step` 循环跑到 `SQLITE_DONE`），之后
`next()/first()/seek()/at()/value()` 全部只操作这份内存缓冲，不再碰 `sqlite3_stmt`
——任意行跳转因此是 O(1) 数组下标，不需要"reset+重新 step N 次"这类取巧实现。

### `PkSqlQuery(PkString)` 单参构造函数——"构造即执行"语义（Task 2 补做，原计划未跑完）

**背景**：plan 阶段这条探针因工具环境间歇性不可用没跑完，`KisResourceCacheDb.cpp:1608-1611`：
```cpp
QString sql = f.readAll();     // "INSERT INTO storage_types (name) VALUES(?)"
QSqlQuery q(sql);               // <- 单参构造：explicit QSqlQuery(const QString& query = QString(), QSqlDatabase db = QSqlDatabase())
q.addBindValue(name);
q.exec();
```
Qt 文档对这个构造函数的描述是"若 `query` 非空会立即执行"——原本担心它会把未绑定
的 `?` 当 NULL 静默插入一条多余的行。

**Task 2 跑通探针**，命令：
```bash
QT=/home/qiansenwei/workspace/Krita_Linux_liyang/krita-ci-env/_install
g++ -fPIC -std=c++17 sql_probe3.cpp -o sql_probe3 \
  -I$QT/include -I$QT/include/QtCore -I$QT/include/QtSql \
  -L$QT/lib -lQt5Sql -lQt5Core -Wl,-rpath-link,$QT/lib
QT_QPA_PLATFORM=offscreen LD_LIBRARY_PATH=$QT/lib QT_PLUGIN_PATH=$QT/plugins ./sql_probe3
```

原始输出：
```
[0] bare QSqlQuery(db) baseline: lastError().isValid()= false  type()= 0
[0b] direct exec(unbound ?) ok= false  lastError= "Parameter count mismatch"  numRowsAffected= -1
[0b] row count after direct exec(unbound ?) = 0
[1] after single-arg construct, before addBindValue/exec:
    isActive()= false  isSelect()= false  lastQuery()= "INSERT INTO storage_types (name) VALUES(?)"  lastError().isValid()= true
    lastError type()= 2  text()= "Parameter count mismatch"  databaseText()= ""  driverText()= "Parameter count mismatch"  nativeErrorCode()= ""
    numRowsAffected()= -1
    row count in table BEFORE addBindValue/exec() call = 0
[2] after addBindValue+exec(): execOk= true  lastError= ""
    final row 0 : id= 1  name= QVariant(QString, "kritaBundle")
[3] final row count = 1  (若单参构造未额外执行应为1；若有构造即执行副作用应为2且第一行name=NULL)
```

**结论**：单参构造函数**确实会尝试立即执行**（构造后 `lastError().isValid()=true`、
`isActive()=false`，与"什么都没做"的基线 `[0]` 不同）——但原来的担心不成立：`[0b]`
独立验证了 sqlite3/Qt 对参数个数不匹配（1 个 `?`，0 个已绑定值）是**直接拒绝执行**
（`"Parameter count mismatch"`），不是把 `?` 当 NULL。所以构造时的隐式执行尝试
必然失败且**没有任何可观察副作用**（`[1]` 显示调用之后表里仍是 0 行）——之后
`addBindValue`+显式 `exec()` 才是真正插入那一行的操作，`[3]` 最终行数=1，确认没有
多插入 NULL 行。

**对 `pk/sql` 的落地结论**：`PkSqlQuery(PkString)` 单参构造函数**不需要复刻
"构造即执行"这个副作用**——`addStorageType()` 是这个构造函数唯一的真实调用点，
且构造之后、`addBindValue`/显式 `exec()` 之前不读取任何状态，所以无论内部是"只
`prepare(sql)`"还是"`prepare` 后再尝试一次注定失败的 `exec`"，从调用点角度观察到
的最终效果完全一致（只插入一行，值正确）。**实现成"内部调用 `prepare(sql)`，不
`exec()`"**——更简单，也不会为了忠实复刻一个不可观察的失败尝试而多做一次无意义
的 prepare+step 往返。已由 Task 3 落地（见 §3）。

**另补一条 Task 2 自己需要的探针**（`value(PkString name)` 的限定名查找）：直接用
vendored sqlite3 C API（不需要 Qt）针对 `KisResourceLocator.cpp:225-237` 的真实
`SELECT tags.id, tags.url, ..., resource_types.id FROM tags, resource_types ...`
语句跑 `sqlite3_column_name`，结果：

```
column_count=8
[0] name=[id]              ← tags.id
[1] name=[url]
[2] name=[active]
[3] name=[name]
[4] name=[comment]
[5] name=[filename]
[6] name=[resource_type]
[7] name=[id]              ← resource_types.id，与 [0] 同名冲突
```

**`sqlite3_column_name` 报出的是裸列名，不带表前缀**——`"tags.id"`/
`"resource_types.id"` 这两个字面串在结果列名里根本不存在。也就是说
`KisResourceLocator.cpp:260-261` 的 `query.value("tags.id")`/
`query.value("resource_types.id")` 在真实 Qt/SQLite 环境下**也查不到列、返回
Invalid**——核对了这两行赋值给的局部变量 `tagId`/`resourceTypeId`，函数其余部分
（`tagForUrlNoCache()`）从未读取过它们，是已经存在于 Krita 里的死代码，不是
`pk/sql` 引入的偏差。`pk/sql` 的 `PkSqlQuery::value(PkString)` 按裸列名精确匹配
实现（第一个同名列优先，对应第 265 行 `value("id")` 命中 `tags.id` 那一列的真实
行为），原样复刻，不做"智能识别限定名"的特殊处理。

### 位置批量 `execBatch` 探针（Task 3 补做，`sql_probe4_execbatch_positional.cpp`）

**背景**：§0 P4 只验证了具名批量路径，位置批量（`KisResourceCacheDb::deleteStorage()`
的 `addBindValue(QVariantList) + execBatch()` 形态）与"跨行失败"这两个语义
Task 3 自己补探针核实。

编译/运行命令：
```bash
QT=/home/qiansenwei/workspace/Krita_Linux_liyang/krita-ci-env/_install
g++ -fPIC -std=c++17 sql_probe4_execbatch_positional.cpp -o sql_probe4 \
  -I$QT/include -I$QT/include/QtCore -I$QT/include/QtSql \
  -L$QT/lib -lQt5Sql -lQt5Core -Wl,-rpath-link,$QT/lib
QT_QPA_PLATFORM=offscreen LD_LIBRARY_PATH=$QT/lib QT_PLUGIN_PATH=$QT/plugins ./sql_probe4
```

原始输出：
```
[1] all-success execBatch ok= true  numRowsAffected= 1  lastError.type= 0  lastError.isValid= false
    remaining row count= 1
[2] partial-failure execBatch ok= false  numRowsAffected= -1
[3] lastError.type= 1  isValid= true  text= "UNIQUE constraint failed: t.name Unable to fetch row"  nativeErrorCode= "19"
    survivor row: id= 10  name= "ten"
[5] named execBatch ok= true  numRowsAffected= 1
```

- **[1]**：位置批量 `DELETE FROM t WHERE id = ?` + `addBindValue([2,3,4])`，
  三行全部成功删除（`remaining row count=1` 印证：种子 3 行 + 原有 1 行 = 4
  行，删掉 3 行剩 1 行），`numRowsAffected()=1`——**不是** 3（三行删除的总数），
  是**最后一次** `sqlite3_changes()` 的值（单行 DELETE 影响 1 行）。这条钉死
  了"不跨行聚合 numRowsAffected"的实现决策。
- **[2]/[3]**：位置批量插入 3 行（id=10/11/12，name 中第 2 个"existing"与已有
  行冲突），`execBatch()` 整体 `ok=false`，`numRowsAffected()=-1`（与单次
  `exec()` 失败时的取值规则一致），`lastError().type()=1`
  （`ConnectionError`，与 P1 的 UNIQUE 冲突分类完全一致——`execBatch` 内部
  某一行失败时的错误分类走的是同一条 `fromStepFailure` 路径，不是新分类）。
  **`survivor row` 只有 id=10 一行**——确认第 2 行失败后**没有继续跑第 3 行**
  （id=12 那行插入从未发生），验证了"停在第一条失败的行"这条实现假设。
- **[5]**：具名批量对照，`ok=true`，`numRowsAffected()=1`（同 [1] 的
  "最后一行"规则，不是跨行累加）。

**落地结论**：`execBatch()`（两种绑定方式）第一行失败即停，不继续跑剩余行，也
不整体回滚（没有隐式事务包裹）；返回值/`lastError()`/`numRowsAffected()` 全部
取自"第一次失败"或"全部成功后最后一次" `execInternal()` 的结果，不做跨行聚合。

---

## 1. 用量表（判据①：范围上界）

**口径**：`libs/`（不含 `tests/`）里命中 `QSqlDatabase|QSqlQuery|QSqlError|
QSqlRecord|QSqlField|QSqlDriver` 的非测试文件，现场数：
```
$ grep -rlE "QSqlDatabase|QSqlQuery|QSqlError|QSqlRecord|QSqlField|QSqlDriver" \
    --include=*.cpp --include=*.h . | grep -v /3rdparty | grep -v /tests/
```
**16 文件**（`libs/brush/KisBrushTypeMetaDataFixup.cpp` + 15 个 `libs/resources/` 下的
文件，含 `KisDatabaseTransactionLock.{h,cpp}`/`KisResourceCacheDb.cpp`/
`KisResourceLocator.cpp`/`KisResourceMetaDataModel.cpp`/`KisResourceModel.cpp`/
`KisResourceQueryMapper.{h,cpp}`/`KisResourceTypeModel.cpp`/`KisSqlQueryLoader.{h,cpp}`/
`KisStorageModel.{h,cpp}`/`KisTagModel.cpp`/`KisTagResourceModel.cpp`），**188 处**方法调用
（任务描述给的 192 是写文档那一刻的口径，现场重数取 188 为准——四位数以内的出入，
按线级 spec「一切数字现场数」不追究）。`QSqlRecord`/`QSqlField`/`QSqlDriver`
**全仓 0 处**（`libs/` 排除 tests），**从本任务范围里整体去掉**。

### `QSqlDatabase` → `PkSqlDatabase`

| 方法 | 次数 | 是否实现 |
|---|---:|---|
| `addDatabase("QSQLITE")`（唯一驱动名） | 1 | 是——固定假设 SQLite，`driverType` 参数可忽略 |
| `setDatabaseName()` | 1 | 是 |
| `open()` | 2 | 是 |
| `close()` | 1 | 是 |
| `PkClose()`（checked close） | R-33 cleanup 互操作 | 是——成功才释放 handle；BUSY 保留 handle/事务状态 |
| `isOpen()` | 1 | 是 |
| `isValid()` | 1 | 是 |
| `QSqlDatabase::database(connName, open)`（静态，两参重载，`open=false` 用于"先查
  连接是否已存在且可用，不自动开"，`KisResourceCacheDb::createDatabase()` 用它避免
  重复开连接） | 22 | 是——因为单连接模型，`connName` 参数忽略，只保留 `open` 参数 |
| `QSqlDatabase::connectionNames()`（静态） | 1 | 是 |
| `tables()` | 2 | 是 |
| `transaction()`/`commit()`/`rollback()` | 6/6/6（含经 `KisDatabaseTransactionLock`
  间接调用的） | 是 |
| `lastError()` | 6 | 是 |
| `databaseName()`/`removeDatabase()`/`isDriverAvailable()`/`driverName()` | 0 | **不实现**（判据①"一项不多"） |

`close()` 保持 Qt 兼容的 `void` 表面；需要可靠 cleanup 的消费者应调用 `PkClose()`。
它返回 `true` 表示连接已关闭或本来没有 handle，返回 `false` 表示 SQLite 拒绝关闭（例如
BUSY），此时 handle 与事务标志仍由 `PkSqlDatabase` 持有。正确的失败生命周期顺序是：调用方
先执行 poison（策略不在 `pk/sql`）→ `rollback()` → query `clear()` 或析构以 finalize
statement → 重试 `PkClose()`。本任务的 SQLite 探针原始输出显示 `sqlite3_close()` 在
事务和 pending `SELECT 1` statement 存在时返回 `SQLITE_BUSY`（十进制 `5`）；释放 statement
后重试成功。本覆盖仅验证 checked primitive 与生命周期，不实现 S-02-b consumer poison
状态机。

### `QSqlQuery` → `PkSqlQuery`

| 方法 | 次数 | 形态 | 是否实现 |
|---|---:|---|---|
| `bindValue(":name", PkVariant)` | 171 | 100% 具名占位符，0 处位置 `bindValue(int,…)` | 是 |
| `bindValue(":name", PkVariantList)` | 属于 execBatch 具名批量（§0） | 具名批量 | 是 |
| `addBindValue(PkVariant)` | 13 | 位置占位符 `?` | 是 |
| `addBindValue(PkVariantList)` | execBatch 位置批量 | 位置批量 | 是 |
| `prepare(PkString)` | 81 | | 是 |
| `exec()`（`prepare` 后无参数） | ~108 | | 是 |
| `exec(PkString)`（一次性，无需先 `prepare`，含 `PRAGMA`/`CREATE TABLE`/`.sql`
  脚本单条语句） | 数十处（`KisSqlQueryLoader` 内部循环调用） | | 是 |
| `execBatch(BatchExecutionMode::ValuesAsRows)` | 5（1 直接 + 4 经
  `KisSqlQueryLoader::execBatch()`包装） | 具名/位置两种 list 都要支持 | 是 |
| `next()` | 21 | | 是 |
| `first()` | 38 | | 是 |
| `seek(int)` | 6 | **必须支持任意行跳转**（§0 P6） | 是 |
| `at()` | 3 | | 是 |
| `clear()` | 4 | | 是 |
| `isValid()` | 1 | | 是 |
| `isSelect()` | 1 | | 是 |
| `setForwardOnly(bool)` | 3 | 语义上只是提示，`pk/sql` 内部游标实现不强依赖它 | 是（存字段，允许无操作） |
| `size()` | 1 | **恒返回 `-1`**（§0 P5，死代码调用点） | 是（trivial） |
| `lastInsertId()` | 1 | 返回 `PkVariant`（`sqlite3_last_insert_rowid()`） | 是 |
| `numRowsAffected()` | 4 | `sqlite3_changes()` | 是 |
| `lastQuery()` | 2 | | 是 |
| `executedQuery()` | 2 | 与 `lastQuery()` 语义等价（Qt 里两者绝大多数情况下相同，
  `pk/sql` 可共用一份存储） | 是 |
| `boundValues()` | 40（全部用途是 `qWarning()` 诊断打印，不参与逻辑分支） | 返回一个可
  遍历的绑定值集合 | 是（最小实现：`PkVariantMap`，key 是占位符名/位置） |
| `value(int)` / `value(PkString name)` | 174（121 具名 + 47 位置，另 6 处经
  `KisSqlQueryLoader::query().value()`） | **具名查找要支持 `KisResourceLocator.cpp`
  的 `"tags.id"`/`"resource_types.id"` 这种带表前缀写法——实测确认这两处在真实
  Qt/SQLite 上本来就查不到列，是既有死代码（§0），`pk/sql` 按裸列名精确匹配即可，
  不做限定名智能识别 | 是 |
| `isActive()`/`finish()`/`driver()`/`isNull(int/name)` | 0 | **不实现** |

### `QSqlError` → `PkSqlError`

| 方法 | 次数 | 是否实现 |
|---|---:|---|
| `text()` | 显式取用 9+ 处，另 ~160 处经 `qWarning() << err` 隐式走 `QDebug operator<<`
  （该 operator 内部调用 `.text()`，本任务不做 `QDebug` 集成——那是 `pk/log` R-08 的
  范围，`pk/sql` 只需保证 `.text()` 本身格式对齐，调用点在 S-02-b 换掉 `qWarning`
  为 `pk/log` 的等价物时自然接上） | 是 |
| `type()` | 11（全部在 `KisResourceCacheDb::initialize()` 一个函数里，对 5 个枚举值
  做穷举 `switch`） | 是——**枚举值必须与 Qt 数值对齐**：`NoError=0`/
  `ConnectionError=1`/`StatementError=2`/`TransactionError=3`/`UnknownError=4`
  （Qt 头文件声明顺序即数值，§0 P1 探针确认前三个的实际取值） |
| `isValid()` | 2 | 是 |
| `nativeErrorCode()`/`databaseText()`/`driverText()` | 0（无生产代码读取，只有
  `text()` 内部拼接用得到 `databaseText`/`driverText`） | **保留内部字段但不强制
  暴露独立测试覆盖**——`text()`的对齐判据已经间接覆盖了它们的正确性，逐项列出
  仅为完整性 |

### 明确不做（范围决策）

- `QSqlRecord`/`QSqlField`/`QSqlDriver`：全仓 0 处，不实现
- `QSqlDatabase` 多连接（具名 connection）：0 处使用，`PkSqlDatabase` 按单一全局
  默认连接设计
- `QSqlQuery::isActive()`/`finish()`/`driver()`/`isNull()`：0 处使用
- 老式 `QSqlDatabase::exec(QString)`（Qt5 已废弃的静态一次性执行）：0 处使用
- `.sql` 脚本本身的搬运/机械改名不在本任务——那是 `KisSqlQueryLoader.cpp` 这个
  **消费方**文件的事，归 S-02-b。本任务只保证 `pk/sql` 的 API 形状能承接它
- `update_from_001.sql`：`sql.qrc` 里注册但没有任何 `.cpp` 引用它、且内容本身
  不是合法 SQL（`INSERT ... VALUES name = "Memory"`、`UPDATE TABLE ...` 都不是
  合法语法），像是早期迁移脚本的死代码——**这不是 `pk/sql` 的范围**（`pk/sql`
  不解析 `.sql` 文件内容），记录在这里供 S-02-b 参考，别把它当成需要保真复刻的
  用例

---

## 2. 工程形态

```
pk/sql/
├── CMakeLists.txt              薄壳工程；find_package(SQLite3) → FetchContent 兜底
├── PkSqlError.h / .cpp         错误类型：ErrorType 枚举 + text()/nativeErrorCode()/…
├── PkSqlDatabase.h / .cpp      单一全局连接门面：open/close/transaction/commit/rollback/tables
├── PkSqlQuery.h / .cpp         prepare/bindValue/addBindValue/exec/execBatch/next/seek/value/…
├── PkSqlCursor.h / .cpp        内部：支撑 seek(int) 任意行跳转的结果集缓冲（§0 P6）
├── compat/QSqlDatabase         垫片：#define QSqlDatabase PkSqlDatabase
├── compat/QSqlQuery            垫片：#define QSqlQuery PkSqlQuery
├── compat/QSqlError            垫片：#define QSqlError PkSqlError
└── tests/
    ├── test_main.cpp / test_*.cpp   PK_* harness 单测（§0 每条探针结论对应一条用例）
    └── graft/
        ├── graft_run_candidate1.sh / graft_run_candidate1_driver.cpp / stubs/   见 §3
```

**sqlite3 依赖**（照抄 `pk/xml/CMakeLists.txt` 的 `find_package`→`FetchContent` 结构）：

```cmake
find_package(SQLite3 QUIET)     # CMake 内建 FindSQLite3.cmake（≥3.14），装了系统 libsqlite3-dev 就命中
if (NOT SQLite3_FOUND)
    include(FetchContent)
    FetchContent_Declare(sqlite3_amalgamation
        URL https://sqlite.org/2025/sqlite-amalgamation-3500400.zip
        URL_HASH SHA256=1d3049dd0f830a025a53105fc79fd2ab9431aea99e137809d064d8ee8356b032)
    FetchContent_MakeAvailable(sqlite3_amalgamation)
    add_library(sqlite3_vendored STATIC ${sqlite3_amalgamation_SOURCE_DIR}/sqlite3.c)
    target_include_directories(sqlite3_vendored PUBLIC ${sqlite3_amalgamation_SOURCE_DIR})
    add_library(SQLite::SQLite3 ALIAS sqlite3_vendored)
endif()
```

**版本与哈希出处**：`sqlite-amalgamation-3500400` = SQLite 3.50.4，是 I-02 Android
三 ABI 依赖流水线已经下载验证过的同版本，本 plan 现场 `sha256sum` 得到上面这个
哈希；与 R-07/pugixml 的"tag+hash 钉死"是同一套做法。

**为什么 vendor amalgamation 而不是 `apt install libsqlite3-dev`**：`pk/sql` 是
fork worktree 里的代码，构建环境不受本任务控制（线级 spec 明确"不 vendor 源码进
fork"针对的是"把源码文件拷进仓库"，`FetchContent` 在 configure 期下载不算 vendor
进 git 历史），而 `find_package` 优先已经覆盖了"系统装了就用系统的"这条路。

---

## 3. 判据②：真实调用点试接结果（Task 3）

**候选筛选**：按依赖轻重排列了 `KisDatabaseTransactionLock` →
`KisSqlQueryLoader` → `KisBrushTypeMetaDataFixup` 三个候选，全部是
`target_link_libraries(kritaresources ...)` 下的真实生产文件（不是自己新写的
测试类）——这是乙类"只交付接口"分支要求的"真实生产源文件编译试接"，不是甲类
的"测试类跑绿"（SQL 访问层没有 1-2 个"专门测 SQL 层"的 Krita 测试类，
`tests/TestResourceCacheDb.cpp` 等测试的是整个 `KisResourceCacheDb`，依赖面比
`pk/sql` 的 `locks` 大得多）。

### 候选 1 —— `libs/resources/KisDatabaseTransactionLock.{h,cpp}`：**编译+链接+跑绿**

试接脚本：`pk/sql/tests/graft/graft_run_candidate1.sh`
驱动源：`pk/sql/tests/graft/graft_run_candidate1_driver.cpp`（探针驱动，不是
driver 降级路径——链接的是候选 1 本身真实、未修改的 `.o`，driver.cpp 只是补一个
"调用它"的 `main()`，因为该类没有专门的 `kis_add_test` 隔离测试类）

依赖满足情况：
- `<QSqlDatabase>` → `pk/sql/compat/QSqlDatabase`（Task 1 交付）
- `<QSqlError>` → `pk/sql/compat/QSqlError`（Task 1 交付）
- `<QDebug>`/`qWarning()` → `pk/log/compat/QDebug`（R-08 已交付，直接复用，
  未改动）
- `<kis_assert.h>` → 复用 `pk/container/tests/graft/stubs/kis_assert.h`
  的内容，在 `pk/sql/tests/graft/stubs/kis_assert.h` 放了一份逐字相同的
  拷贝（与仓库里其余 `pk/*/tests/graft/stubs/` 的既有惯例一致）
- `<KisAdaptedLock.h>` → `libs/global/KisAdaptedLock.h`，真实、未修改的
  Krita 源文件，**零 Qt 依赖**（纯 `<mutex>` + 模板），直接 `-I libs/global`
  引用，不需要垫片
- `<kritaresources_export.h>` → 新建 `pk/sql/tests/graft/stubs/
  kritaresources_export.h`（`#define KRITARESOURCES_EXPORT`，逐字照抄
  `pk/port/graft/stubs/kritaresources_export.h`/`pk/config/tests/graft/
  stubs/kritaresources_export.h` 两份既有先例）

运行命令与原始输出：

```bash
$ bash pk/sql/tests/graft/graft_run_candidate1.sh
  试接跑绿: KisDatabaseTransactionLock graft driver
    [1] after explicit commit(): count(name='committed')=1 (expect 1)
    [2] after leaving scope without commit(): count(name='rolledback')=0 (expect 0)
    [3] after explicit rollback(): count(name='explicit_rollback')=0 (expect 0)
    [4] sequential transactions: count(a)=1 count(b)=1 (expect 1,1)
    PASS: all KisDatabaseTransactionLock graft assertions held
    nm -u graft_candidate1 | grep -i qt: 无输出
    git diff --quiet -- libs/resources libs/global: 干净
graft_exit=0
```

驱动覆盖了 `KisDatabaseTransactionLock.h` 类文档描述的完整 RAII 契约（真实、
未修改的 `KisDatabaseTransactionLockAdapter::lock()/unlock()/commit()`，逐行
未改动）：
- `[1]` 构造即 `lock()`（`std::unique_lock` 构造语义），显式 `commit()` 后
  数据提交保留
- `[2]` 不调用 `commit()`、离开作用域析构——自动 `unlock()` 回滚，数据不落库
- `[3]` 显式 `KisDatabaseTransactionLock::rollback()`（转发到 `unlock()`），
  效果同 `[2]`，且随后析构不重复 `unlock()`（`std::unique_lock` 自己的
  `owns_lock` 状态防重入）
- `[4]` 两次独立顺序事务互不干扰（`m_transactionStarted` 状态不跨实例残留）

**判据②靠此候选达成**：零改动编译真实生产文件 + 链接真实 `.o` + 跑绿 +
`nm -u | grep -i qt` 无输出 + `git diff --quiet` 干净，五项全部满足。**没有走
driver 降级路径**——candidate 1 succeeds，判据提前满足，没有再走候选 2/3 的
完整 graft 流程（没有编出对应产物、没有链接、没有跑测试）。

### 候选 2 —— `libs/resources/KisSqlQueryLoader.{h,cpp}`：轻量探针确认依赖墙

补充记录用的轻量探针（单条 `g++ -c` 命令，不是完整 graft 脚本），命令：

```bash
$ g++ -std=c++17 -DQT_NO_DEBUG \
  -I pk/sql/tests/graft/stubs -I pk/sql -I pk/sql/compat -I pk/string -I pk/string/compat \
  -I pk/variant -I pk/container -I pk/geometry -I pk/log -I pk/log/compat \
  -I libs/global -I libs/resources \
  -c libs/resources/KisSqlQueryLoader.cpp -o /tmp/KisSqlQueryLoader_probe.o
```

原始输出：

```
In file included from libs/resources/KisSqlQueryLoader.cpp:7:
libs/resources/KisSqlQueryLoader.h:14:10: fatal error: QStringList: 没有那个文件或目录
   14 | #include <QStringList>
      |          ^~~~~~~~~~~~~
compilation terminated.
```

卡在 `KisSqlQueryLoader.h` 第 14 行 `#include <QStringList>`——本 worktree
目前没有任何 R 任务交付 `QStringList` 的 compat 垫片。plan 预判的依赖墙
（`QFile`/`QRegularExpression`/`QtSql` 全量）连边都没摸到就先撞上更早的
`QStringList`（`.h` 里排在它们前面），但结论一致：**候选 2 编不过**，卡在
未交付能力，不是 `pk/sql` 范围内的缺口。归属：`libs/resources` 剥 Qt 的
`S-02-b`。

### 候选 3 —— `libs/brush/KisBrushTypeMetaDataFixup.cpp`：轻量探针确认依赖墙

```bash
$ g++ -std=c++17 -DQT_NO_DEBUG \
  -I pk/sql/tests/graft/stubs -I pk/sql -I pk/sql/compat -I pk/string -I pk/string/compat \
  -I pk/variant -I pk/container -I pk/geometry -I pk/log -I pk/log/compat \
  -I libs/global -I libs/brush -I libs/resources \
  -c libs/brush/KisBrushTypeMetaDataFixup.cpp -o /tmp/KisBrushTypeMetaDataFixup_probe.o
```

原始输出：

```
In file included from libs/brush/KisBrushTypeMetaDataFixup.cpp:7:
libs/brush/KisBrushTypeMetaDataFixup.h:10:10: fatal error: kritabrush_export.h: 没有那个文件或目录
   10 | #include "kritabrush_export.h"
      |          ^~~~~~~~~~~~~~~~~~~~~
compilation terminated.
```

卡在自己模块的导出头（本任务没有为 `libs/brush` 建这个占位——不在候选 1/2/3
的"依赖轻重"排序里预期要建的东西，plan 预判的真正依赖墙是 `KisResourceLocator.h`，
这个更早的导出头缺口只是提前暴露同一个结论：**候选 3 编不过**）。归属：同候选 2，
`libs/resources`/`libs/brush` 剥 Qt 的 S 批次。

---

## 4. 已知偏离清单 / concerns

### 4.1 `pk/variant/CMakeLists.txt` 的 `add_subdirectory` 双路径冲突（既有 bug，未修）

**这是一个真实存在、独立于本任务的既有 bug**（Task 2 开工时发现）。独立复现
（不需要 `pk/sql` 参与）：

```
$ cmake -S pk/variant -B /tmp/pkvariant_test_build -G Ninja
CMake Error at pk/test/CMakeLists.txt:25 (add_subdirectory):
  The binary directory
    /tmp/pkvariant_test_build/pkstring
  is already used to build a source directory. It cannot be used to build
  source directory
    .../pk/string.
```

根因：`pk/variant/CMakeLists.txt` 自己同时 `add_subdirectory(pk/string)`（直接）
与 `add_subdirectory(pk/geometry)`（间接，经 `pk/geometry → pk/test →
pk/string`）——而 `pk/test/CMakeLists.txt:25` 写的是
`add_subdirectory(${PKSTRING_DIR} ${CMAKE_BINARY_DIR}/pkstring …)`，用的是**顶层**
`CMAKE_BINARY_DIR` 而不是 `CMAKE_CURRENT_BINARY_DIR`，所以不管嵌套多深都落在同一个
二进制目录，与 `pk/variant` 自己那次直接 `add_subdirectory` 撞车。这个 bug 在
`pk/variant` 自己单独 configure 时就会炸，与 `pk/sql` 是否参与无关。

**没有修那两个文件**（在本任务范围外——brief 明确"不要碰 `pk/sql` 之外的目录"）。
绕法：`pk/sql/CMakeLists.txt` 里手写 `pkgeometry`/`pkvariant` 两个静态库目标，
直接把 `pk/geometry/*.cpp`、`pk/variant/{PkAuxTypes,PkVariant}.cpp` 加进源文件
列表，**不 `add_subdirectory` 那两个目录**——只读了它们的源文件（编译进自己定义
的目标），一个字节没改 `pk/variant/`、`pk/geometry/`、`pk/test/` 下任何文件。
target 名字、公开 include 路径、编译选项（`-fwrapv`）都照抄各自原本的
CMakeLists.txt，保证下游看到的目标形状不变，并用 `if (NOT TARGET pkgeometry)`/
`if (NOT TARGET pkvariant)` 包住避免将来别的 `pk/*` 任务也这么绕时重复定义撞车。

**遗留**：这个 bug 本身没有修——`pk/variant/CMakeLists.txt`（或
`pk/test/CMakeLists.txt:25` 把 `${CMAKE_BINARY_DIR}` 改成
`${CMAKE_CURRENT_BINARY_DIR}`）需要它们各自归属的任务/后续任务来修。两条修法
哪个更对，留给 `pk/variant`/`pk/test` 归属的任务判断，不是本任务该拍板的。

### 4.2 单参构造函数 `PkSqlQuery(const PkString &query)` 的补齐时机

不在 `task-3-brief.md` 字面清单里，是根据 plan §0 末尾的显式指派（"Task 3 补这个
构造函数时，实现成'内部调用 `prepare(sql)`，不 `exec()`'即可"）在 Task 3 补齐的
——已知、已裁决，只是没写进 brief 摘要，不是自行扩大范围。落地见 §0 对应小节与
§3。

### 4.3 其他实现细节（非探针实测值，标注为"合理选择"而非"复刻 Qt"）

- `bindVariantAtIndex` 对"占位符名字在语句里不存在"（`sqlite3_bind_parameter_index`
  返回 0）选择静默跳过而不是报错——没有真实调用点会撞上这种情况（都是绑
  语句里确实存在的占位符），是防御性选择，不是探针实测的行为。
- `seek(index)` 对负下标的处理（钉在 BeforeFirstRow，返回 false）不是 §0 探针
  覆盖的场景，是按"越界"统一处理的合理选择。
- `boundValues()` 的最小实现（`PkVariantMap`，key 是占位符名/位置）只保证 40 处
  `qWarning()` 诊断打印能用，未对齐 Qt `QVariantMap` 在具名+位置混用场景下的
  精确 key 格式（真实调用点没有混用场景）。

---

## 5. 判据③终验（Task 4，完整产物，含 `PkSqlQuery`/`execBatch` 全部代码路径）

命令与原始输出：

```bash
$ cd pk/sql
$ source /home/qiansenwei/workspace/Krita_Linux_liyang/krita-ci-env/env
$ ninja -C build
ninja: Entering directory `build'
ninja: no work to do.
ninja exit=0

$ ctest --test-dir build --output-on-failure
Internal ctest changing into directory: .../pk/sql/build
Test project .../pk/sql/build
    Start 1: test_pksql
1/1 Test #1: test_pksql .......................   Passed    0.29 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.38 sec
ctest exit=0

$ nm -u -C build/libpksql.a | grep -i qt
（无输出，grep exit=1）
```

`build/libpksql.a` 此时包含全部四个编译单元
（`PkSqlError.cpp.o`/`PkSqlDatabase.cpp.o`/`PkSqlQuery.cpp.o`/`PkSqlCursor.cpp.o`，
`ar t build/libpksql.a` 实测确认），`PkSqlQuery.cpp.o` 含 `execBatch` 全部代码
路径——**判据③在完整产物范围内成立**，与 Task 1/2/3 各自的中间核验（分别只
覆盖当时已交付的子集）不同，这是终版记录。

---

## 6. 全量单测结果（Task 4；§8 全分支评审修复轮后重跑，见该节原始输出）

`./build/test_pksql` 完整输出（三个 `QObject` 测试类各自的 Totals 行，Task 4
交付时的版本，35 条）：

```
********* Start testing of TestError *********
PASS   : TestError::initTestCase()
PASS   : TestError::noErrorIsInvalid()
PASS   : TestError::syntaxErrorIsStatementError()
PASS   : TestError::tableNotFoundIsStatementError()
PASS   : TestError::prepareTimeSyntaxErrorIsStatementError()
PASS   : TestError::uniqueConflictIsConnectionError()
PASS   : TestError::primaryKeyConflictIsConnectionError()
PASS   : TestError::notNullConflictIsConnectionError()
PASS   : TestError::cleanupTestCase()
Totals: 9 passed, 0 failed, 0 skipped
********* Finished testing of TestError *********
********* Start testing of TestDatabase *********
PASS   : TestDatabase::initTestCase()
PASS   : TestDatabase::openIsOpenClose()
PASS   : TestDatabase::databaseOpenFalseDoesNotCloseAlreadyOpenConnection()
PASS   : TestDatabase::connectionNamesReflectsAddDatabase()
PASS   : TestDatabase::tablesListsUserTablesOnly()
PASS   : TestDatabase::transactionCommitSucceedsWithNoError()
PASS   : TestDatabase::nestedTransactionFailsWithTransactionError()
PASS   : TestDatabase::cleanupTestCase()
Totals: 8 passed, 0 failed, 0 skipped
********* Finished testing of TestDatabase *********
********* Start testing of TestQuery *********
PASS   : TestQuery::initTestCase()
PASS   : TestQuery::namedBindValueInsertsRow()
PASS   : TestQuery::positionalBindValueInsertsRow()
PASS   : TestQuery::prepareOnceLoopBindExecReusesStatement()
PASS   : TestQuery::execOneShotSelectReadsRows()
PASS   : TestQuery::valueBeforeNextIsInvalidNoError()
PASS   : TestQuery::valueAfterNextExhaustedIsInvalidNoError()
PASS   : TestQuery::valueColumnIndexOutOfRangeIsInvalid()
PASS   : TestQuery::sizeIsAlwaysMinusOneRegardlessOfForwardOnly()
PASS   : TestQuery::seekJumpsBackwardAfterForwardOnlyNext()
PASS   : TestQuery::seekJumpsBackwardAfterRandomAccessNext()
PASS   : TestQuery::namedValueLookupIsUnqualifiedColumnNameOnly()
PASS   : TestQuery::clearResetsToEmptyQueryReadyForReprepare()
PASS   : TestQuery::execBatchNamedValuesAsRowsInsertsAllRows()
PASS   : TestQuery::execBatchPositionalValuesAsRowsMatchesDeleteStorageShape()
PASS   : TestQuery::execBatchStopsAtFirstFailingRowLikeRealQtDriver()
PASS   : TestQuery::singleArgConstructorPreparesWithoutExecuting()
PASS   : TestQuery::cleanupTestCase()
Totals: 18 passed, 0 failed, 0 skipped
********* Finished testing of TestQuery *********
test_pksql exit=0
```

**合计 35 passed, 0 failed, 0 skipped**（`TestError` 9 + `TestDatabase` 8 +
`TestQuery` 18；`TestQuery` 18 条里 14 条是 Task 2 交付，4 条是 Task 3 新增的
execBatch/单参构造函数用例）。`ctest --test-dir build` 层面是 `1/1 Test`（整个
`test_pksql` 可执行文件算一个 ctest 用例，内部再展开成上面 35 条 `QObject` 子
用例）。**这是 Task 4 交付时的快照，之后 §8 全分支评审修复轮新增了 3 条
（`TestDatabase` +1、`TestQuery` +2），当前实际总数是 38——以 §8 的原始输出
为准，本节不回填，保留原始交付记录。**

---

## 7. 剥离笔记

### 踩的坑

1. **文档记的探针环境路径读不到**：`krita/AGENTS.md`/线级 spec 写的
   `/mnt/ssd-disk/liyang/projects/krita-ci-env/env` 是另一个用户的私有副本
   （权限 `600`），本项目执行账号读不到——本 plan 与本 worktree 现场发现并
   订正为 `/home/qiansenwei/workspace/Krita_Linux_liyang/krita-ci-env/env`，
   见 §0 开头。**这条对本仓库任何需要探针实测的后续任务都成立**，不只是
   `pk/sql`。
2. **`pk/variant/CMakeLists.txt` 的 `add_subdirectory` 双路径冲突**（§4.1）：
   Task 2 引入 `PkVariant` 依赖时才暴露，根因是 `pk/test/CMakeLists.txt:25`
   用了 `${CMAKE_BINARY_DIR}` 而不是 `${CMAKE_CURRENT_BINARY_DIR}`。绕过手段
   是手写目标而不 `add_subdirectory`，**没有修复这个既有 bug**——留给
   `pk/variant`/`pk/test` 归属的任务。
3. **`type()` 分类是反直觉的**：UNIQUE/PK/NOT NULL 约束冲突被 Qt 驱动分类成
   `ConnectionError` 而不是看起来更合理的 `StatementError`（§0 P1）。如果
   凭直觉实现（"约束冲突当然是语句错误"）会实现错，`KisResourceCacheDb::
   initialize()` 里那个对 5 个 `ErrorType` 分支处理人类可读消息的 `switch`
   就会走错分支。
4. **`value(PkString)` 的两处限定名调用点是死代码**：`KisResourceLocator.cpp`
   的 `value("tags.id")`/`value("resource_types.id")` 在真实 Qt 环境下就已经
   查不到列（`sqlite3_column_name` 报的是裸列名），赋值给的局部变量后续从未
   被读取。第一反应可能会想"要不要支持限定名查找让这两行工作起来"——**不该
   这么做**，那样反而与真实 Qt 行为不一致，是当前实现故意保留的偏离方向
   （复刻死代码的死性，不是修复它）。

### 哪些是猜的（不是探针实测，需要留意）

- §4.3 列出的三条：`bindVariantAtIndex` 对不存在占位符的静默跳过、`seek()`
  负下标的处理、`boundValues()` 的最小实现形态——真实调用点没有触达这几种
  边界情况，是按"合理默认"实现的，不是从 Qt 行为逆向出来的。
- `PkSqlQuery(PkString)` 单参构造函数虽然探针证实了"构造即执行尝试但无可观察
  副作用"，但"实现成只 `prepare` 不 `exec`"这个具体选择本身是基于"两种实现从
  调用点角度等价，选更简单的"这条工程判断，不是被探针唯一钉死的（探针只排除
  了"复刻构造即执行会插入多余 NULL 行"这一种错误实现，没有反过来证明"只
  prepare"是 Qt 内部真实做法——Qt 内部很可能确实执行了一次注定失败的
  `sqlite3_step`，`pk/sql` 选择不复刻这次无意义的失败尝试）。

### 留给 S-02-b 处理

- 候选 2（`KisSqlQueryLoader.{h,cpp}`）撞 `QStringList` compat 缺失。
- 候选 3（`KisBrushTypeMetaDataFixup.cpp`）撞自己模块导出头
  `kritabrush_export.h` 缺失（更深层还有 `KisResourceLocator.h` 这堵真正的墙）。
- `.sql` 脚本本身的搬运/机械改名（`KisSqlQueryLoader.cpp` 消费方文件的事，
  见 §1「明确不做」）。
- `#include <QSqlDatabase>`/`<QSqlQuery>`/`<QSqlError>` 换成 `pk/sql` compat
  垫片的机械替换本身——16 个生产文件的实际改动。
- `qWarning() << err`（~160 处隐式走 `QDebug operator<<`）在换成 `pk/log` 的
  等价物时，需要确认 `PkSqlError` 与 `pk/log` 的集成点（本任务只保证
  `.text()` 格式对齐，不做 `QDebug` 集成，见 §1「`QSqlError::text()`」条目）。

---

## 附：`KisDatabaseTransactionLock` 完整源码（候选 1 试接的复刻蓝本）

`libs/resources/KisDatabaseTransactionLock.cpp`（57 行，全文）：

```cpp
KisDatabaseTransactionLockAdapter::KisDatabaseTransactionLockAdapter(QSqlDatabase database)
    : m_database(database)
{
}

void KisDatabaseTransactionLockAdapter::lock()
{
    KIS_SAFE_ASSERT_RECOVER_RETURN(!m_transactionStarted);
    if (!m_database.transaction()) {
        qWarning() << "WARNING: Failed to start a transaction:" << m_database.lastError().text();
    } else {
        m_transactionStarted = true;
    }
}

void KisDatabaseTransactionLockAdapter::unlock()
{
    if (!m_transactionStarted) return;
    if (!m_database.rollback()) {
        qWarning() << "WARNING: Failed to rollback a transaction:" << m_database.lastError().text();
    }
    m_transactionStarted = false;
}

void KisDatabaseTransactionLockAdapter::commit()
{
    if (!m_transactionStarted) return;
    if (!m_database.commit()) {
        qWarning() << "WARNING: Failed to commit a transaction:" << m_database.lastError().text();
    }
    m_transactionStarted = false;
}
```

行为：构造/`lock()` 开事务；析构（若未显式 `commit()`）走 `unlock()` 回滚；显式
`.commit()` 提交并清除"待回滚"标记——是"默认回滚、必须显式提交"的 RAII 守卫，
`pk/sql` 不需要在 `PkSqlDatabase` 里内建这个包装（`KisDatabaseTransactionLock`
本身是 Krita 自己的类，不是 Qt 类型，不在 `pk/sql` 的 compat 范围内——它消费
`PkSqlDatabase::transaction()/commit()/rollback()` 三个方法即可正常工作，
S-02-b 换掉 `#include <QSqlDatabase>` 为 `pk/sql` 的 compat 垫片后这个文件本身
大概率不用改一个字）。候选 1 试接（§3）已实测证明这份源码零改动可以直接对着
`pk/sql` 的 compat 垫片编译+链接+跑绿。

---

## 8. 全分支评审修复轮（2026-08-18，Important #1 / #2 + Minor 五条）

R-17 4 个 Task 各自通过 task-scoped 评审后，跑了一次全分支终审，发现 2 条
Important 级问题，本节记录修复内容与依据。**不覆盖上面 §1–§7 的原始交付记录**
（§6 的 35 条快照保留不动），本节是追加的修复轮记录。

### 8.1 Important #1：`boundValues()` 承诺已交付但代码里根本不存在

§1 用量表与 §4.3 一直把 `boundValues()` 描述成"是（最小实现）"，但 `PkSqlQuery`
上此前**根本没有这个方法**（`grep -in "bound" pk/sql/*.h pk/sql/*.cpp` 零匹配）
——40 处真实调用点（全部是 `qWarning() << q.boundValues()` 诊断打印，不参与
逻辑分支）会在 S-02-b 接上时直接编译失败。

**修复**：在 `PkSqlQuery`（`PkSqlQuery.h`/`PkSqlQuery.cpp`）上补上

```cpp
PkVariantMap boundValues() const;
```

实现直接复用现有存储（`m_namedBinds` 原样并入结果；`m_positionalBinds` 按下标
转成十进制字符串 key "0"/"1"/... 补进去），与 §1/§4.3 原来的描述（"`PkVariantMap`，
key 是占位符名/位置"）一致——**这条描述本身是对的，只是此前没有对应实现**，
本轮补上后 §1/§4.3 不需要改文字。

新增单测（`pk/sql/tests/test_query.h`/`.cpp`）：
- `TestQuery::boundValuesReturnsNamedBinds`：具名 `bindValue(":name", ...)` 后
  `boundValues()` 能按 `":name"` 查到值。
- `TestQuery::boundValuesReturnsPositionalBindsByStringIndex`：位置
  `addBindValue(...)` 两次后 `boundValues()` 能按 `"0"`/`"1"` 查到对应值，
  且 `size()==2`。

### 8.2 Important #2：`PkSqlQuery::exec()` 失败是否传播进 `PkSqlDatabase::lastError()`

真实 `KisResourceCacheDb.cpp` 428-436/450-457 行有 `if (!q.exec()) { ...;
return db.lastError(); }` 这种形态——返回的是**连接级** `lastError()`，不是
`q` 自己的。`pk/sql` 现有实现里 `PkSqlQuery::execInternal()` 只写自己的
`m_lastError`，`PkSqlDatabase` 的单例状态 `state().lastError` 只被
`open()`/`transaction()`/`commit()`/`rollback()` 写——从未被一次 query 失败
写过。这条语义此前**没有探针裁定**：如果真实 Qt SQLite 驱动会把 query 失败
传播到连接级 `lastError()`，`pk/sql` 不传播就是一个真回归。

**探针**：`/tmp/.../scratchpad/sql_probe5_lasterror_propagation.cpp`（环境
`/home/qiansenwei/workspace/Krita_Linux_liyang/krita-ci-env/env` 的可读副本，
手动 `-I`/`-L` 方式，命令与 §0 开头一致）。起一个 `:memory:` `QSqlDatabase`，
分别用语法错误（`SELCT`）、UNIQUE 约束冲突（照抄 `addBindValue+exec` 形态）、
以及直接复刻 428-436 行的 `return db.lastError();` 写法三种场景，紧接失败的
`q.exec()` 之后读 `db.lastError()`。原始输出（完整）：

```
[0] db.open() = true
[0] db.lastError() BEFORE any query: isValid= false  type= 0  text= ""
[A] syntax-error exec ok= false
[A] q.lastError(): isValid= true  type= 2  text= "near \"SELCT\": syntax error Unable to execute statement"
[A] db.lastError() immediately after: isValid= false  type= 0  text= ""
[A] db.lastError() == q.lastError() ? type match= false  text match= false
[B] success exec ok= true
[B] db.lastError() after a successful query: isValid= false  type= 0  text= ""
[C] constraint-violation exec ok= false
[C] q.lastError(): isValid= true  type= 1  text= "UNIQUE constraint failed: t.id Unable to fetch row"
[C] db.lastError() immediately after: isValid= false  type= 0  text= ""
[C] db.lastError() == q.lastError() ? type match= false  text match= false
[D] q.exec() failed as expected, q.lastError().text()= "near \"SELCT\": syntax error Unable to execute statement"
[D] simulate `return db.lastError();`: dbErr.isValid()= false  dbErr.type()= 0  dbErr.text()= ""
[D] dbErr == qErr (would caller see the real failure)?  false
```

**结论：不传播。** 无论语法错误还是约束冲突，`db.lastError()` 在 query 失败前后
恒为 `isValid()=false`/`type()=NoError`（`[0]`/`[A]`/`[B]`/`[C]` 全部一致），
与 `q.lastError()`（失败时的真实错误）完全独立。`[D]` 直接印证真实
`KisResourceCacheDb.cpp` 里 `return db.lastError();` 这个写法在真实 Qt 环境下
**本来就拿不到失败原因**——这是 Krita/Qt 既有行为里的一个反直觉点（这个
返回值实际上恒为 NoError，调用方 `initialize()` 的 `switch (err.type())`
会把这类失败误判成 `NoError` 分支），不是 `pk/sql` 会引入的新偏差。

**落地**：`pk/sql` 现有实现（`PkSqlQuery` 从不触碰 `PkSqlDatabase` 的单例状态）
已经与这个结论一致，**不需要改代码**——同 §4.3/§7"哪些是猜的"里"死代码调用点"
的处理方式（P6 先例）：原样复刻，不"修复"这个反直觉行为。

新增单测：`TestDatabase::queryFailureDoesNotPropagateToConnectionLevelLastError`
（`pk/sql/tests/test_database.h`/`.cpp`）——分别用语法错误与约束冲突两种失败
场景，钉住 `db.lastError()` 全程保持 `NoError`/`!isValid()`，防止未来有人以为
这是遗漏而"顺手"给 `PkSqlDatabase` 加传播逻辑。

### 8.3 Minor 五条（记录，不改代码）

3. `PkSqlDatabase::transaction()`/`commit()`/`rollback()`（`PkSqlDatabase.cpp`）
   不像 `tables()` 那样防御 `state().handle == nullptr`——低风险，`sqlite3_exec`
   对 `nullptr` db 参数本身是 NULL-safe（不会崩，会返回错误码），只是没有像
   `tables()` 那样提前短路返回空结果。
4. `pk/sql/CMakeLists.txt` 里手抄 `pkgeometry`/`pkvariant` 源文件列表（§4.1
   绕开 `add_subdirectory` 双路径冲突的手段）有漂移风险：`pk/variant`/
   `pk/geometry` 未来若新增源文件，这份手抄列表不会自动跟上，需要人工同步。
5. SQL 里合法的 `NULL` 值与"未定位"/"越界"访问在当前实现里返回同一个 invalid
   `PkVariant`（`columnValue()`/`PkSqlCursor::value()` 对 `SQLITE_NULL` 与
   "没有当前行"/"下标越界"用的是同一份"空 `PkVariant`"表示），调用方用
   `isValid()`/`isNull()` 无法区分"这一列本来就是 NULL"和"你没定位到有效行/
   列不存在"——这是 `pk/variant` 自身没有"NULL 值"与"无效/未定位"两种独立
   状态这个设计决定的下游后果，不是 `pk/sql` 能在自己范围内改的，S-02-b 若
   有逻辑依赖这个区分需要另想办法。

### 8.4 重跑验证（本轮修复后，完整产物）

```bash
$ cd pk/sql
$ source /home/qiansenwei/workspace/Krita_Linux_liyang/krita-ci-env/env
$ ninja -C build
ninja: Entering directory `build'
ninja: no work to do.
ninja exit=0

$ ctest --test-dir build --output-on-failure
Internal ctest changing into directory: .../pk/sql/build
Test project .../pk/sql/build
    Start 1: test_pksql
1/1 Test #1: test_pksql .......................   Passed    0.01 sec

100% tests passed, 0 tests failed out of 1

Total Test time (real) =   0.01 sec
ctest exit=0

$ nm -u -C build/libpksql.a | grep -i qt
（无输出，grep exit=1）
```

`./build/test_pksql` 完整输出（合计 **38 passed, 0 failed, 0 skipped**：
`TestError` 9 + `TestDatabase` 9（原 8 + 本轮新增 1）+ `TestQuery` 20（原 18 +
本轮新增 2）——新增的 3 条全部在下面输出里）：

```
********* Start testing of TestError *********
PASS   : TestError::initTestCase()
PASS   : TestError::noErrorIsInvalid()
PASS   : TestError::syntaxErrorIsStatementError()
PASS   : TestError::tableNotFoundIsStatementError()
PASS   : TestError::prepareTimeSyntaxErrorIsStatementError()
PASS   : TestError::uniqueConflictIsConnectionError()
PASS   : TestError::primaryKeyConflictIsConnectionError()
PASS   : TestError::notNullConflictIsConnectionError()
PASS   : TestError::cleanupTestCase()
Totals: 9 passed, 0 failed, 0 skipped
********* Finished testing of TestError *********
********* Start testing of TestDatabase *********
PASS   : TestDatabase::initTestCase()
PASS   : TestDatabase::openIsOpenClose()
PASS   : TestDatabase::databaseOpenFalseDoesNotCloseAlreadyOpenConnection()
PASS   : TestDatabase::connectionNamesReflectsAddDatabase()
PASS   : TestDatabase::tablesListsUserTablesOnly()
PASS   : TestDatabase::transactionCommitSucceedsWithNoError()
PASS   : TestDatabase::nestedTransactionFailsWithTransactionError()
PASS   : TestDatabase::queryFailureDoesNotPropagateToConnectionLevelLastError()
PASS   : TestDatabase::cleanupTestCase()
Totals: 9 passed, 0 failed, 0 skipped
********* Finished testing of TestDatabase *********
********* Start testing of TestQuery *********
PASS   : TestQuery::initTestCase()
PASS   : TestQuery::namedBindValueInsertsRow()
PASS   : TestQuery::positionalBindValueInsertsRow()
PASS   : TestQuery::prepareOnceLoopBindExecReusesStatement()
PASS   : TestQuery::execOneShotSelectReadsRows()
PASS   : TestQuery::valueBeforeNextIsInvalidNoError()
PASS   : TestQuery::valueAfterNextExhaustedIsInvalidNoError()
PASS   : TestQuery::valueColumnIndexOutOfRangeIsInvalid()
PASS   : TestQuery::sizeIsAlwaysMinusOneRegardlessOfForwardOnly()
PASS   : TestQuery::seekJumpsBackwardAfterForwardOnlyNext()
PASS   : TestQuery::seekJumpsBackwardAfterRandomAccessNext()
PASS   : TestQuery::namedValueLookupIsUnqualifiedColumnNameOnly()
PASS   : TestQuery::clearResetsToEmptyQueryReadyForReprepare()
PASS   : TestQuery::execBatchNamedValuesAsRowsInsertsAllRows()
PASS   : TestQuery::execBatchPositionalValuesAsRowsMatchesDeleteStorageShape()
PASS   : TestQuery::execBatchStopsAtFirstFailingRowLikeRealQtDriver()
PASS   : TestQuery::singleArgConstructorPreparesWithoutExecuting()
PASS   : TestQuery::boundValuesReturnsNamedBinds()
PASS   : TestQuery::boundValuesReturnsPositionalBindsByStringIndex()
PASS   : TestQuery::cleanupTestCase()
Totals: 20 passed, 0 failed, 0 skipped
********* Finished testing of TestQuery *********
test_pksql exit=0
```

`build/libpksql.a` 含全部四个编译单元（`ar t build/libpksql.a` 未重跑，§5
Task 4 已确认过、本轮未新增编译单元）——**判据③在本轮修复后仍然成立**，
0 个 Qt 符号，35 条既有测试全部保持绿（+3 条新增）。
