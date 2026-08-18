# pk/xml —— Q-2（XML）零 Qt 替代（Task 1：pugixml 集成 + `PkXml` DOM 核心）

本目录交付 `QDom*` 一族（`QDomDocument`/`QDomElement`/`QDomNode`/`QDomNodeList`/
`QDomText`/`QDomCDATASection`/`QDomAttr`/`QDomImplementation`/`QDomDocumentType`）
的零 Qt 替代，底层用 [pugixml](https://github.com/zeux/pugixml) v1.16
（commit `c8033ce9d039e7f9d134877c363397b3cfe20816`，`git ls-remote --tags` 实测
的真实最新稳定 tag，见本任务 commit message 里贴的原始输出）。

`QXmlStream*`（`PkXmlStreamReader`/`PkXmlStreamWriter`）是 Task 2 的范围，不在
本目录（本任务只交付 CMake 里 pugixml 的接线，Task 2 复用它但不消费本任务的
C++ 类型——见线级 plan `docs/superpowers/plans/R-07.md` §0 preflight 的
Task1→Task2 结论）。Task 2 已交付，见文末「Task 2 已知偏离」。

## 1. 内部表示

见 `PkXmlNode.h` 顶部注释——概括：`_node` 是 pugixml 的 `pugi::xml_node` 值句柄
（轻量指针，不持有内存），`_doc` 是指向 `pugi::xml_document` 的
`std::shared_ptr`（持有整棵树的内存池，且用 shared_ptr 保证这个堆上对象的地址
终生不变——pugixml 把首页内存结构体嵌在 `xml_document` 对象自身里，对象一旦
被拷贝/移动内部指针就会失效）。`PkXmlAttr` 额外持有一个 `pugi::xml_attribute`
（pugixml 把属性设计成与节点完全独立的类型，不像 Qt DOM 那样纳入同一个继承
体系）。

## 2. 已知偏离清单

### 2.1 `createElement()` 返回的节点在真正 appendChild 之前，`parentNode()` 不是 null

**根因**：pugixml 的 `allow_move()`（`src/pugixml.cpp`，实测源码确认）硬性要求
`parent.root() == child.root()`——跨 `xml_document` 的 `append_move`/
`insert_move_before` 一律静默失败、返回空节点。唯一跨文档转移内容的手段是
`append_copy()`（深拷贝），但深拷贝会产生新的节点身份：Krita 里最常见的写法

```cpp
QDomElement e = doc.createElement("foo");
parent.appendChild(e);
e.setAttribute("x", "1");   // 期望改在真正挂树的那份上
```

如果 `createElement()` 返回真正悬空（不属于任何 `xml_document`）的节点，深拷贝
方案会让第三行的 `setAttribute` 静默地改在孤儿副本上——不可接受的正确性问题。

**取舍**：`createElement()`/`createTextNode()`/`createCDATASection()` 把新节点
立即挂成 `PkXmlDocument` 自己那棵 `pugi::xml_document` 树的（文档根的）子节点，
`appendChild()`/`insertBefore()` 用 `append_move()`/`insert_move_before()` 在
**同一棵树内部**搬动它——因为始终同一棵树，`allow_move()` 恒成立，且 pugixml
的节点句柄只是指向同一个底层结构体的指针，搬动之后调用方手里那份旧
`PkXmlElement` 自动"看见"新位置，不需要额外的身份同步代码。

**代价**：一个刚创建、还没被 `appendChild`/`insertBefore` 挂到目标位置的节点，
`parentNode()` 会返回文档本身，而不是 Qt 那样的 null。真实调用点一律是"创建后
立刻 appendChild"，不存在中间态被查询的场景，所以这条偏离不影响试接
（Task 3/4）。

### 2.2 `toByteArray()` 退化成 `PkString`（UTF-8 编码），不是 `PkByteArray`

brief 的 Interfaces 签名是 `PkByteArray toByteArray(int indent = 1) const`，但
本 worktree 当前**没有**交付 `PkByteArray`——已现场确认 `pk/container/` 下不存在
任何 `*ByteArray*` 文件（`R-02` 尚未产出）。按 brief 关键实现说明 5 给的退化
路径：`PkXmlDocument::toByteArray()` 直接返回 `toString()` 的结果（UTF-8 编码
内容），行为等价。

真实调用点 23 处用到 `toByteArray`（用量表 §1）。若后续 `R-02` 交付
`PkByteArray`，这里的签名要跟着改——**这是已知技术债，不是本任务遗漏**，
Task 2（`PkXmlStreamWriter` 构造函数同样依赖 `PkByteArray` 的可用性判断，见
plan §Task2 Step "PkByteArray 可用性"）需要知道这一点：**Task 1 交付时确认
`PkByteArray` 不可用**，Task 2 应该同样走 `PkString*` 的退化路径，不要重新
判断一遍。

### 2.3 `setContent` 的错误信息文本不追求逐字节对齐 Qt，但 `errorLine`/`errorColumn` 是正确算出来的 1-based 值

pugixml 是完全不同的解析器实现，`xml_parse_result::description()` 的措辞与 Qt
的 DOM 解析器不同（例如探针 P4 的 `<root><a></root>` 这个标签不匹配的例子，
Qt 报 `"tag mismatch"`，pugixml 实测报 `"Start-end tags mismatch"`——语义一致，
文本不同）。`errorLine`/`errorColumn` 则是本任务自己在 `xml_parse_result::offset`
（字节偏移量）基础上扫一遍已解析前缀数换行/列算出来的，**必须正确**——已用
上面这个例子实测确认：`offset=11` 时 `line=1 col=12`（1-based），`test_document.cpp`
的 `setContentReportsErrorOnMalformedXml` 断言的就是这两个实测值。

### 2.4 属性输出顺序：插入顺序，不是 Qt 的哈希桶序

探针 P2 实测 Qt 的属性顺序不是插入序也不是字典序，是
`QDomNamedNodeMap`/`QHash` 内部哈希桶序，未文档化、不保证跨版本/跨输入稳定。
`PkXmlElement::setAttribute()` 用 pugixml 原生行为——插入顺序——这是"比 Qt
更确定"的行为改进，不是弱化，按 plan §0 P2 的判定不需要刻意复刻 Qt 的哈希
桶序。

### 2.5 注释 / 处理指令 / XML 声明不作为节点保留

`setContent` 用 `pugi::parse_default | pugi::parse_doctype`（额外加
`parse_doctype` 是为了让 `doctype()` 能从 setContent 解析出的 DOCTYPE 声明里
取到 name()——`parse_default` 本身不含它，已现场探针确认）。`parse_default`
本身不含 `parse_comments`/`parse_pi`/`parse_declaration`——用量表（`docs/超`
`plans/R-07.md` §1）里 `QDomComment`/`QDomProcessingInstruction` 实测 **0
处**，Task 1 的类清单里也没有这两个类型，注释/处理指令/`<?xml ?>` 声明因此
不会出现在解析出的树里、也不会计入 `childNodes()`。若后续任务发现真实调用点
需要它们，属于新缺口，需要另开任务补 `PkXmlComment`/对应解析标志。

### 2.6 `PkXmlNodeList` 是一次性快照，不是 Qt 的实时视图

Qt 的 `QDomNodeList` 是"实时"的——底层树变了，之前取到的 `QDomNodeList` 遍历
结果也会跟着变。`PkXmlNodeList` 是取一次时的 `std::vector<PkXmlNode>` 快照，
之后树的增删不会反映到已经取出的列表里。真实调用点没有发现依赖"取到列表后
继续改树、期望列表也跟着变"这种写法（`elementsByTagName`/`childNodes` 的真实
调用点都是"取一次、马上遍历"），这条偏离目前判断不影响试接，Task 3/4 试接时
如果撞到反例需要更新本节。

### 2.7 P9 转义规则：已验证与 Qt 完全一致，不是偏离

探针 P9（`a<b>c&d"e'f` → `a&lt;b>c&amp;d&quot;e'f`：只转义 `<`/`&`，属性值里
再加 `"`，不转义 `>`/`'`）已用本机 pugixml v1.16 实测复现，逐字节相同，
**不需要**任何额外处理——记在这里是为了让后续任务知道这条已经验证过，不用
重新怀疑。

## 3. 命名空间处理范围

pugixml 本身不做命名空间解析（`xmlns:foo="uri"` 被当成普通属性，`foo:tag`
被当成字面 tag name）。`PkXmlElement::localName()`/`namespaceURI()`/
`attributeNS()` 手动拆 `prefix:localname`、沿祖先链查最近的 `xmlns[:prefix]`
属性解析 URI——只覆盖真实调用点用到的固定前缀场景（`KoXmlNS::draw`/
`KoXmlNS::svg`/`KoXmlNS::manifest` 等，见用量表「命名空间 API」小节，4 处），
不是完整 XML Namespaces 规范实现（例如不处理运行时动态换绑同一前缀到不同 URI
这类边缘场景）。

## 4. 依赖获取方式

`pk/xml/CMakeLists.txt` 先 `find_package(pugixml QUIET)`，本 worktree 当前
没有系统装的 pugixml（`pkg-config --exists pugixml` 实测为假），走
`FetchContent` 下载 pugixml v1.16 源码编译（不 vendor 进 fork）。若后续 I-02
之类的步骤把 pugixml 装进依赖前缀，`find_package` 分支自动生效，本文件不用改。

## 5. Task 2 已知偏离（`PkXmlStreamReader`/`PkXmlStreamWriter`/`PkXmlStreamAttributes`）

### 5.1 `PkXmlStreamWriter` 构造函数接 `PkString*`，不是 brief 原始签名的 `PkByteArray*`

与 §2.2 `PkXmlDocument::toByteArray()` 同一个决定，不是重新判断一遍——`PkByteArray`
本 worktree 未交付，Task 1 交付时已现场确认。

### 5.2 `PkXmlStreamAttributes::hasAttribute()` 是补的接口，不在 brief 的 Interfaces 枚举里

实现前对真实调用点的用量复核发现 `libs/pigment/resources/KoColorSet.cpp`
（`colorProperties.hasAttribute("RGB")` 等 2 处）与
`plugins/assistants/Assistants/kis_painting_assistant.cpp`（`xml.attributes()
.hasAttribute(...)` 共 4 处）都在真实调用 `QXmlStreamAttributes::hasAttribute()`，
但线级 plan 的方法级用量表把 `hasAttribute | 108` 整行记在 `PkXmlElement`
名下（疑似把这几处 stream 侧调用点也并进了同一个计数）。按
`CLAUDE.md`「新发现的缺口直接补进来」原则补上这个方法，不是自创接口——
详见 `PkXmlStreamAttributes.h` 类注释。

### 5.3 `PkXmlStreamReader` 不消费 Task 1 的 `PkXmlNode`/`PkXmlDocument` 等类型

只复用 pugixml 这个库本身（`#include <pugixml.hpp>`），内部有自己的一份
`pugi::xml_node → PkString` 转换小工具（`PkXmlStreamReader.cpp` 里的
`pkStreamFromUtf8`），没有 `#include "PkXmlNode.h"` 去复用它现成的
`pkXmlFromPugi()`——与本文件顶部「Task 2 复用它但不消费本任务的 C++ 类型」
的既定范围一致。

### 5.4 `PkXmlStreamReader::raiseError()` 是补的接口，线级 plan 的用量表把它记漏了

线级 plan（`docs/superpowers/plans/R-07.md` 方法级用量表）记
`raiseError`「实测 0 调用点，不实现」，但现场 `grep -rn raiseError`
（排除 `pk/xml/`）实测 `libs/pigment/resources/KoColorSet.cpp` 有 **6 处**
`xml->raiseError(...)`——是漏统计，不是真的没有调用点。按 `CLAUDE.md`
「新发现的缺口直接补进来，不算改决策」原则补上。**这条建议主会话核实后
同步进 `docs/superpowers/plans/R-07.md` 的用量表**（本任务只读该文件，
不能自己改）。

**精确语义已用本机 Qt 5.15.7（`libQt5Core.so.5.15.7`）的实测探针核实**
（评审 Important，见 `.superpowers/sdd/R-07/task-2-report.md`「raiseError
语义核实」一节，不是猜的）：调用 `raiseError(message)` 后**不需要再调用
`readNext()`**，`tokenType()` 立刻变成 `Invalid`、`atEnd()`/`hasError()`
立刻变成 `true`、`errorString()` 立刻是传入的 `message`——但
`name()`/`text()`/`attributes()` **不会被清空**，仍然是报错前最后一次成功
`readNext()` 留下的值。之后任意多次 `readNext()` 都恒返回 `Invalid`，状态
冻结、不再继续遍历——覆盖了评审指出的场景：`KoColorSet.cpp` 的 6 处
`raiseError()` 后紧跟 `return`，但外层调用点未必严格遵守
`while (!xml.atEnd())` 惯用法。见 `test_stream_reader.cpp` 的
`raiseErrorSetsErrorStateAndAtEnd` 用例。

### 5.5 `PkXmlStreamWriter`/`PkXmlStreamReader` 都不可拷贝/不可移动

`PkXmlStreamReader` 显式 `= delete` 拷贝与移动构造/赋值——原因见
`PkXmlStreamReader.h` 类注释（`_doc` 是唯一持有的 `pugi::xml_document` 值，
真实调用点从不拷贝/移动 `QXmlStreamReader` 实例）。`PkXmlStreamWriter` 没有
显式 delete，但也没有实现拷贝/移动语义的正确性保证（`_output` 是外部指针、
`_buf`/`_openElements` 是值成员，默认拷贝/移动能编译但语义上两份 writer 会
共享同一个 `_output` 目标——真实调用点同样是原地构造使用，未发现需要拷贝/
移动的场景，暂不处理，若后续试接撞到需要更新本节）。

### 5.6 两个不在 brief Interfaces 里、但对齐 Qt 只读接口的小 getter

`PkXmlStreamAttributes::size()`（`count()` 的别名转发，同 `PkXmlNodeList::size()`
的做法——`QXmlStreamAttributes` 本身是 `QVector<QXmlStreamAttribute>` 的别名，
`size()` 是 `QVector` 自带的方法）与 `PkXmlStreamWriter::autoFormatting()`
（`setAutoFormatting(bool)` 的只读对偶，Qt 原生就有这个 getter）都不在 brief
的 Interfaces 枚举里，但都是照抄 Qt 对应类型现成接口的最小补全，不是自创行为
——评审记为 Minor，登记在这里保持与 §5.2/§5.4 同样的「补的接口都要登记」
一致性。

## 6. Task 3 试接（候选 A：`kis_dom_utils.cpp`）新增/修正

见 `.superpowers/sdd/R-07/task-3-report.md` 完整报告。这里只记两处改了
Task 1/2 交付面本身的：

- **`PkXmlNode` 新增 `firstChildElement`/`lastChildElement`/
  `nextSiblingElement`/`previousSiblingElement` 四个便捷方法**（原来只有
  `PkXmlElement` 上有）。真 Qt 的 `QDomNode` 自 Qt 4.1 起就自带这四个方法
  （不需要先转成 `QDomElement`），`kis_dom_utils.cpp` 的
  `findElementByAttribute(QDomNode parent, ...)` 直接对形参类型是
  `QDomNode`（未转型）的 `parent` 调用 `parent.firstChildElement(tag)`——
  这是真实、常见的 Qt 用法，Task 1 交付时只在 `PkXmlElement` 上给了这四个
  方法。算法与 `PkXmlElement.cpp` 同名方法逐字一致（跳过非元素兄弟/子节点），
  实现挪到 `PkXmlNode.cpp`。
- **`pk/xml/compat/QDomElement` 补上对 `QDomDocument`/`QDomNodeList` 的转发
  include**。真 Qt 的 `<QDomElement>` 转发到 QtXml 模块的单一头文件
  `qdom.h`，那个头把 `QDomImplementation`/`QDomNode`/`QDomDocumentType`/
  `QDomDocument`/`QDomNodeList`/… 整个家族一次性声明全，所以真实调用点常见
  "只 `#include <QDomElement>` 一个，之后裸用 `QDomDocument`/`QDomNodeList`"
  这种写法——`kis_dom_utils.cpp` 正是如此：只 `#include <QDomElement>`，
  函数体里却直接用 `QDomDocument doc = ...` 与 `QDomNodeList list = ...`。
  此前 `compat/QDomElement` 只转发 `QDomNode` 一个，覆盖不了这条真实存在的
  Qt 传递性。
