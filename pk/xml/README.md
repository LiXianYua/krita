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

## 0. 探针实测（判据 0，逐条原始输出）

**从线级 plan `docs/superpowers/plans/R-07.md` §0 完整搬运**（Task 4 收尾要求，
供后续任务/S 线消费者查阅 XML 语义时的权威依据——plan 文件不在 fork 仓库里，
S 线任务的 worktree 读不到它，这里是唯一能读到的副本）。

**探针环境**：`source /mnt/ssd-disk/liyang/projects/krita-ci-env/env`，
`QT=/mnt/ssd-disk/liyang/projects/krita-ci-env/_install`（Qt 5.15.7，含
`libQt5Xml.so.5.15.7`）。编译命令：

```bash
g++ -fPIC -std=c++17 probe.cpp -o probe \
  -I$QT/include -I$QT/include/QtCore -I$QT/include/QtXml \
  -L$QT/lib -lQt5Xml -lQt5Core -Wl,-rpath-link,$QT/lib
LD_LIBRARY_PATH=$QT/lib:$LD_LIBRARY_PATH ./probe
```

`ldd probe | grep -i qt` 实测确认链的是真 `libQt5Xml.so.5`/`libQt5Core.so.5`（不是
compat 垫片）。以下每条都是原任务的原始输出，不是转述：

### P1：元素间纯空白文本节点——默认**不保留**

```cpp
QDomDocument doc;
doc.setContent(QString("<root>\n  <a>1</a>\n  <b>2</b>\n</root>"));
QDomElement root = doc.documentElement();
// root.childNodes().count() 实测 = 2（不是天真假设的 5：text,a,text,b,text）
```

输出：`root.childNodes().count()=2`，两个子节点都是 `<a>`/`<b>` 元素本身，纯空白
`Text` 节点被 Qt 的 DOM 解析器丢弃。**对齐要求**：`PkXmlElement::childNodes()` 也
不能保留纯空白文本节点。**pugixml 天然对齐**：pugixml 默认解析标志
（`parse_default`）**不含** `parse_ws_pcdata`，同样丢弃纯空白 PCDATA——这条不需要
额外代码对齐，只需要**不要**手滑传 `parse_ws_pcdata` 标志。

### P2：属性输出顺序——**不是**插入顺序，是 Qt 内部实现细节

```
输入插入顺序: z, a, m
doc.toString(-1) 实际输出: <e a="2" z="1" m="3"/>
```

Qt 的属性顺序既不是插入序也不是字典序，是 `QDomNamedNodeMap`/`QHash` 内部哈希桶
序，**未文档化、不保证跨版本/跨输入稳定**。**判定：可接受的偏离**——实测证明
Qt 自己都不保证这个顺序稳定（换一组属性名顺序就变），任何真实调用点如果依赖
属性输出顺序，在 Qt 上本来就是脆弱的——`PkXmlElement` 选择**插入顺序**
（pugixml 原生行为，且更强的保证），这是"比 Qt 更确定"的行为改进，不是弱化。
不需要刻意复刻 Qt 的哈希桶序。

### P3：`toString(indent)` 精确格式

```
toString(4) = "<root>\n    <child k=\"v\">hello</child>\n</root>\n"
toString(0) = "<root>\n<child k=\"v\">hello</child>\n</root>\n"
toString(-1) = "<root><child k=\"v\">hello</child></root>"   （无结尾换行）
```

`indent >= 0` 时结尾带一个 `\n`；`indent < 0`（含 `toString()` 默认值 1，注意默认值
**不是** -1，是 1）走单行紧凑模式且无结尾换行。`PkXmlDocument::toString(int indent = 1)`
默认值是 `1`（Qt 默认），`indent < 0` 与 `indent == -1` 等价视为紧凑模式（实现用
`< 0` 判断，不是 `== -1`）。

### P4：`setContent` 错误报告格式

```cpp
doc.setContent(QString("<root><a></root>"), &errMsg, &errLine, &errCol);
// ok=0 errMsg="tag mismatch" line=1 col=16
```

`errLine`/`errCol` 是 1-based。pugixml 的 `xml_parse_result` 给 `offset`（字节偏移量，
不是行列），实现在 offset 基础上扫一遍已解析前缀自己数换行/列。

### P5：`attribute()` 默认值语义

```
missing-attr default: "DEF"      （key 不存在，给了默认值 → 返回默认值）
empty-attr (no default arg): ""  （key 存在但值为空串，不传默认值 → 返回空串）
hasAttribute(nope)=0  hasAttribute(empty)=1
```

`attribute(name, defaultValue="")` 与 `hasAttribute(name)` 语义独立、不能合并实现
（不能用"返回值是否等于默认值"判断 key 存在与否）。

### P6：CDATA 往返

```
<root><![CDATA[a<b>&c]]></root>
```

CDATA 段内容原样保留（`<`、`&`、`>` 都不转义），pugixml 原生支持 CDATA 节点类型
（`parse_cdata` 在默认标志里），行为天然对齐。

### P7：`elementsByTagName` 是**递归全子树**查找，不是仅直接子元素

```cpp
doc.setContent("<root><a><x/></a><b><x/></b><x/></root>");
doc.documentElement().elementsByTagName("x").count();  // = 3
```

三个 `<x/>` 全部命中（嵌套在 `<a>`/`<b>` 里的两个 + 顶层一个）。
`PkXmlNode::elementsByTagName(name)` 自己写一次前序遍历（DFS walk），返回顺序是
文档序。

### P8：`QDomElement::text()` 是**递归拼接全部后代文本**，不只是直接子文本

```cpp
doc.setContent("<e>foo<sub>ignored</sub>bar</e>");
doc.documentElement().text();  // = "fooignoredbar"
```

嵌套在 `<sub>` 里的 "ignored" 也被拼进结果。`PkXmlElement::text()` 递归遍历全部
后代 text 节点按文档序拼接，不能只扫直接子节点。

### P9：转义规则——只转义 `<`、`&`；属性值里额外转义 `"`；**不转义 `>` 和 `'`**

```
输入属性值: a<b>c&d"e'f  → 输出: a&lt;b>c&amp;d&quot;e'f
输入文本节点: x<y&z       → 输出: x&lt;y&amp;z
```

`>` 与单引号 `'`（双引号属性值场景下）Qt **不转义**（XML 规范也不强制要求）。
**实测：已用本机 pugixml v1.16 复现，逐字节相同**——见下方「已知偏离清单」P9
一节，不是偏离。

### P10：`QXmlStreamWriter` 默认（无 `autoFormatting`）与 `autoFormatting(true)`

```
无 autoFormatting: <?xml version="1.0" encoding="UTF-8"?><root a="1"><child>hi</child></root>\n
autoFormatting=true（默认缩进 4 空格）:
  <?xml version="1.0" encoding="UTF-8"?>
  <root>
      <child>hi</child>
  </root>
```

默认关闭格式化，写紧凑单行（但**结尾仍有一个换行**）；`setAutoFormatting(true)`
后默认缩进 4 空格（真实调用点没有一处改过这个值，硬编码 4）。

### P11：`QXmlStreamReader` token 序列与 `tokenType()` 枚举值

```
<root a="1"><child>text</child></root> 的 readNext() 序列：
token=2(StartDocument) name="" text=""
token=4(StartElement)  name="root"  text=""
token=4(StartElement)  name="child" text=""
token=6(Characters)    name=""      text="text"
token=5(EndElement)    name="child" text=""
token=5(EndElement)    name="root"  text=""
token=3(EndDocument)   name=""      text=""
hasError=0
```

`QXmlStreamReader::TokenType` 实测枚举值：`NoToken=0 Invalid=1 StartDocument=2
EndDocument=3 StartElement=4 EndElement=5 Characters=6`（Qt 头文件还有
`Comment=7 DTD=8 EntityReference=9 ProcessingInstruction=10`，真实调用点用量表
里没有出现这四个）。`PkXmlStreamReader` 的 `TokenType` 枚举数值逐一对齐这份
实测。

### P12：属性值里的数字精度——DOM 层本身不做任何数值格式化

```cpp
e.setAttribute("v", QString::number(0.1 + 0.2, 'g', 17));
// e.attribute("v") = "0.30000000000000004"
```

`setAttribute`/`attribute()` 只是原样存取字符串，数值格式化完全是调用方的责任
——`PkXmlElement::setAttribute`/`attribute` 不需要对数值做任何特殊处理，只需要
是 `PkString` 的直通存取。

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

## 7. 判据①用量表（范围上界，从线级 plan §1 完整搬运）

### 口径

`git ls-files '*.cpp' '*.h' '*.cc'`，排除路径含 `tests/`/`benchmarks/`/`sdk/tests/`
的文件（当前 worktree 是「保留范围」的第一步收窄——D 线已删除的目录本来就不在
`git ls-files` 里了：`libs/ui`、`libs/widgetutils`、`plugins/python`、`libs/libkis`
在本 worktree 里已经不存在）。逐 token `grep -oh -w` 统计类名/方法名出现次数。

**与 `docs/Qt替代品选型.md` §6.2 的口径差异**：选型文档记「`QDom*` 1935 处/
272 文件、`QXmlStream*` 37 处/6 文件」，是全仓早期挖掘的数字。Task 1 在当时
worktree（D 线部分已合并）实测：**保留范围内非测试文件 292 个含 `QDom*`、18 个含
`QXmlStream*`**（全仓含测试再加 20 个）。文件数比选型文档记的 272 略高，量级一致、
方向合理——按"对不上时先怀疑自己的口径"处理，本任务按当前 worktree 实测的
292/18 文件立项，差异已记入 Task 1 report 供主会话核实登记决策文档。

### 类名分布（保留范围、非测试文件，逐类出现次数）

| 类 | 出现次数 |
|---|---:|
| `QDomElement` | 1240 |
| `QDomDocument` | 628 |
| `QDomNode` | 93 |
| `QDomText` | 24 |
| `QDomNodeList` | 8 |
| `QDomCDATASection` | 8 |
| `QDomImplementation` | 1 |
| `QDomDocumentType` | 1 |
| `QDomAttr` | 1 |
| `QXmlStreamReader` | 40 |
| `QXmlStreamWriter` | 20 |
| `QXmlStreamAttributes` | 6 |

`XPath`/`QXmlQuery`/`QXmlSchema` 实测 **0 处**（与选型文档一致，不实现）。

### 方法级用量表（第二步收窄：whole-word `.method(` 计数，**未逐处核实接收者
类型**——高风险的通用名已单独标注，实现前必须逐处抽样确认接收者是
`QDom*`/`QXmlStream*`。这项"未逐处核实"的 caveat 与收窄方法本身是本用量表
最大的已知弱点：真实收窄工作靠"先按类型 grep 变量声明、再看紧跟的方法调用"，
不是直接信任 grep 出的方法名计数）

DOM 系（`PkXmlDocument`/`PkXmlElement`/`PkXmlNode`/`PkXmlNodeList`/`PkXmlText`/
`PkXmlCDATASection`/`PkXmlAttr`）：

| 方法 | 计数 | 接收者歧义风险 | 归属类型 |
|---|---:|---|---|
| `attribute` | 749 | 低（名字够专） | `PkXmlElement` |
| `setAttribute` | 642 | 低 | `PkXmlElement` |
| `isNull` | 609 | **高**（`QVariant`/智能指针等也有 `isNull`） | `PkXmlNode` 及子类 |
| `toString` | 316 | **高**（几乎所有类型都有 `toString`） | `PkXmlDocument`/`PkXmlNode` |
| `createElement` | 221 | 低 | `PkXmlDocument` |
| `appendChild` | 201 | 低 | `PkXmlNode` |
| `name` | 183 | **高**（大量非 XML 类型有 `name()`） | `PkXmlAttr`/`PkXmlStreamReader` |
| `tagName` | 137 | 低 | `PkXmlElement` |
| `save` | 131 | **高**（绝大部分是无关类型的 `save()`，需逐处核实） | 待核实（很可能不属于本任务） |
| `hasAttribute` | 108 | 低 | `PkXmlElement` |
| `firstChildElement` | 101 | 低 | `PkXmlElement` |
| `toElement` | 92 | 低 | `PkXmlNode` |
| `documentElement` | 82 | 低 | `PkXmlDocument` |
| `text` | 74 | 中（部分是别的类型的 `text()`） | `PkXmlElement`（P8 递归语义） |
| `firstChild` | 67 | 低 | `PkXmlNode` |
| `setContent` | 57 | 低 | `PkXmlDocument` |
| `nextSibling` | 50 | 低 | `PkXmlNode` |
| `writeAttribute` | 48 | 低 | `PkXmlStreamWriter` |
| `nextSiblingElement` | 44 | 低 | `PkXmlElement` |
| `atEnd` | 44 | 低 | `PkXmlStreamReader` |
| `errorString` | 33 | 中（多类型都有） | `PkXmlStreamReader`/`PkXmlDocument`(setContent) |
| `attributes` | 24 | 中 | `PkXmlNode`/`PkXmlStreamReader` |
| `writeStartElement` | 23 | 低 | `PkXmlStreamWriter` |
| `toByteArray` | 23 | 中 | `PkXmlDocument` |
| `writeEndElement` | 16 | 低 | `PkXmlStreamWriter` |
| `nodeName` | 16 | 低 | `PkXmlNode` |
| `childNodes` | 12 | 低 | `PkXmlNode` |
| `localName` | 12 | 低 | `PkXmlElement`（命名空间） |
| `previousSibling` | 11 | 低 | `PkXmlNode` |
| `parentNode` | 11 | 低 | `PkXmlNode` |
| `lastChild` | 11 | 低 | `PkXmlNode` |
| `isElement` | 11 | 低 | `PkXmlNode` |
| `namespaceURI` | 9 | 低 | `PkXmlElement`（命名空间） |
| `createTextNode` | 10 | 低 | `PkXmlDocument` |
| `createCDATASection` | 8 | 低 | `PkXmlDocument` |
| `elementsByTagName` | 7 | 低 | `PkXmlNode`（P7 递归语义） |
| `writeCharacters` | 7 | 低 | `PkXmlStreamWriter` |
| `hasError` | 6 | 低 | `PkXmlStreamReader` |
| `ownerDocument` | 6 | 低 | `PkXmlNode` |
| `attributeNode` | 6 | 低 | `PkXmlElement` |
| `insertBefore` | 5 | 低 | `PkXmlNode` |
| `attributeNS` | 5 | 低 | `PkXmlElement`（命名空间） |
| `toText` | 4 | 低 | `PkXmlNode` |
| `toCDATASection` | 4 | 低 | `PkXmlNode` |
| `removeAttribute` | 4 | 低 | `PkXmlElement` |
| `isCDATASection` | 4 | 低 | `PkXmlNode` |
| `hasChildNodes` | 4 | 低 | `PkXmlNode` |
| `removeChild` | 3 | 低 | `PkXmlNode` |
| `readNext` | 3 | 低 | `PkXmlStreamReader` |
| `isText` | 3 | 低 | `PkXmlNode` |
| `previousSiblingElement` | 2 | 低 | `PkXmlElement` |
| `lastChildElement` | 2 | 低 | `PkXmlElement` |
| `writeStartDocument` | 1 | 低 | `PkXmlStreamWriter` |
| `writeEndDocument` | 1 | 低 | `PkXmlStreamWriter` |
| `toAttr` | 1 | 低 | `PkXmlNode` |
| `createProcessingInstruction` | 1 | 低（唯一调用点，实现为无操作透传即可） | `PkXmlDocument` |
| `doctype` | 1 | 低 | `PkXmlDocument` |

**`count`/`size`/`item` 未列入上表**：grep 出的原始计数分别是 331/2023/3——但
几乎全部是无关类型（`QList::count`/`QString::size`/各种 `item()`）。真实
`QDomNodeList` 用量点靠"先找 `QDomNodeList` 类型的变量声明，再看它调用了哪些
方法"来收窄（Qt 里 `count()`/`size()`/`length()` 三个是同一函数的别名，`at()`/
`item()` 也是别名——只实现一份，起别名转发）。

**明确排除**（实测 0 调用点或决策文档已定的范围外）：`QDomEntity`/
`QDomNotation`/`QDomProcessingInstruction`（除 `createProcessingInstruction` 1
处外无其它调用）/`setAttributeNS`/`hasAttributeNS`/`removeAttributeNS`/
`cloneNode`/`replaceChild`/`insertAfter`/`namedItem`/`createComment`/
`createEntityReference`/`createDocumentFragment`/`toDocument`/`toComment`/
`toDocumentFragment`/`toDocumentType`/`toEntity`/`toEntityReference`/`toNotation`/
`toProcessingInstruction`/`toCharacterData`/`isDocument`/`isAttr`/`isComment`/
`setNodeValue`/`nodeValue`/`nodeType`（不作为公开 API）。

QXmlStream 系：

| 方法 | 计数 | 归属类型 |
|---|---:|---|
| `writeAttribute` | 48 | `PkXmlStreamWriter` |
| `atEnd` | 44 | `PkXmlStreamReader` |
| `errorString` | 33（含 DOM 侧共用） | `PkXmlStreamReader` |
| `writeStartElement` | 23 | `PkXmlStreamWriter` |
| `writeEndElement` | 16 | `PkXmlStreamWriter` |
| `writeCharacters` | 7 | `PkXmlStreamWriter` |
| `hasError` | 6 | `PkXmlStreamReader` |
| `readNext` | 3 | `PkXmlStreamReader` |
| `writeStartDocument` | 1 | `PkXmlStreamWriter` |
| `writeEndDocument` | 1 | `PkXmlStreamWriter` |
| `raiseError`（**Task 2 执行阶段更正新增**，见 §5.4） | 6 | `PkXmlStreamReader` |
| `hasAttribute`（**Task 2 执行阶段更正新增**，见 §5.2） | 2 | `PkXmlStreamAttributes` |
| `name`/`text`/`attributes`/`tokenType`（P11 探针确认的 token 遍历面） | 上表已计 | `PkXmlStreamReader` |

`writeAttributes`（批量）/`writeCDATA`/`writeComment`/`writeTextElement`/
`writeEmptyElement`/`writeDTD`/`readNextStartElement`/`skipCurrentElement`/
`isStartElement`/`isEndElement`/`isCharacters` 实测 **0 调用点**，不实现。

### 命名空间 API（4 处，决策文档 §6.2 已提及）——实测的具体调用点

```
libs/flake/KoPathShapeFactory.cpp:46          e.namespaceURI() == KoXmlNS::draw
libs/resources/KoResourceBundleManifest.cpp:82/99   .localName()/.namespaceURI()
plugins/flake/pathshapes/ellipse/EllipseShapeFactory.cpp:56
plugins/flake/pathshapes/rectangle/RectangleShapeFactory.cpp:76
plugins/flake/pathshapes/star/StarShapeFactory.cpp:132/135
plugins/flake/imageshape/ImageShapeFactory.cpp:68
libs/impex/KisDocument.cpp:2159-2166           QDomImplementation::createDocumentType
                                                + QDomDocument::ParseOption::UseNamespaceProcessing
plugins/impex/libkra/kra_converter.cpp:362     doc.doctype().name()
libs/psdutils/cos/psd_text_data_converter.cpp:1475   QDomAttr（唯一 QDomAttr 直接用例）
```

**`setContent` 的命名空间处理开关**：真实调用点用的是
`QDomDocument::ParseOption::UseNamespaceProcessing`（Qt 5.15 新式 `ParseResult`
API）而不是老式 `setContent(dev, true, &msg, &line, &col)` 的布尔参数——两种
`setContent` 重载都已实现（57 处 `setContent` 里两种调用形式混用）。

## 8. 判据②：两个试接候选的选择理由 + 跨 target 补充尝试结果

### 候选筛选方法

先列出所有 `git ls-files` 里路径含 `tests/` 且引用 `QDom*`/`QXmlStream*` 的文件
（18 个候选），再对每个候选的**生产源文件**（不是测试文件本身）检查是否触达
没有任何 R 任务认领的 Qt 类型——最常见的绊脚石是 **`QColor`**
（`pk/config/README.md` §「`PkConfigColor` 是临时的范围内代打」已明确记录：
`QColor` 不在 R-03 几何范围、不在 R-06 `PkVariant` 范围，目前没有任何 R 任务
认领它的完整替代）。

### 候选清单与筛选结果（实测，非猜测）

| 候选测试文件 | 生产文件 | 结果 |
|---|---|---|
| `libs/image/tests/kis_dom_utils_test.cpp` | `libs/global/kis_dom_utils.cpp` | **选中（候选 A，Task 3）**——8 个测试方法全绿 |
| `libs/image/tests/kis_distance_information_test.cpp` | `libs/image/kis_distance_information.cpp` | **选中（候选 B，本 Task）**——`testInitInfo`（含 `testInitInfoXMLClone`）全绿 |
| `libs/brush/tests/kis_auto_brush_factory_test.cpp` | `libs/brush/kis_auto_brush_factory.cpp` | 拒绝：测试文件自身额外拉入 `KoColor.h`/`KisPaintInformation`/`KisFixedPaintDevice`/`KoColorSpaceRegistry`/`QImage`（笔刷渲染对比） |
| `libs/canvas/tests/kis_grid_config_test.cpp` | `libs/canvas/kis_grid_config.cpp` | 拒绝：生产文件依赖 `Q_GLOBAL_STATIC`/`qRegisterMetaType`/`QMetaType`/`QtMath`/大量 `QColor` 成员 |
| `libs/flake/tests/TestKoDrag.cpp`/`TestSvgParser.cpp`/`TestSvgText.cpp`/`SvgParserTestingUtils.h` | `SvgParser`/`KoShapeGroup`/`KoDocumentResourceManager` | 拒绝：SVG 解析 + 形状系统，依赖闭包巨大 |
| `libs/image/tests/kis_asl_layer_style_serializer_test.cpp` | `kis_asl_layer_style_serializer.cpp` | 拒绝：拉入 `KoAbstractGradient`/`KoStopGradient`/`KisResourceModel` 资源系统 |
| `libs/image/tests/kis_asl_parser_test.cpp` | `asl/kis_asl_*` | 拒绝：拉入 `resources/KoPattern`/`resources/KoStopGradient`（资源系统） |
| `libs/image/tests/kis_mask_generator_test.cpp` | 无独立 `.cpp`——`kis_base_mask_generator.cpp` + 6 个具体子类 | **本 Task 尝试，未跑通**，见下「Step 3 结果」 |
| `libs/pigment/tests/TestKoChannelInfo.cpp`/`TestKoColor.cpp`/`TestKoStopGradient.cpp` | — | 拒绝：`KoColor.h` 颜色系统，天然与 `QColor` 缺口相关 |
| `sdk/tests/KisDumbTransformMaskParams.cpp` | 自身即生产/测试混合体 | 未尝试（见下「未尝试的另外两个备选」） |
| `libs/image/tests/kis_paint_information_test.cpp` | `kis_paint_information.cc` | 未尝试（见下「未尝试的另外两个备选」） |

**两个候选都挂在 `libs/image/tests/` 下（CMake 里都算 `kritaimage` 的测试）**
——没能找到干净、依赖闭包小的"另一个 target"候选（`kritabrush`/`kritacanvas`/
`kritaflake`/`kritapigment` 全部卡在 `QColor`/资源系统/形状系统等未交付类型上，
本 Task 尝试的 `kis_mask_generator_test.cpp` 同样在 `kritaimage` 内、卡在另一类
未交付基础设施上，见下）。**这是本任务自认的一个弱点，不是回避**：spec 原文
"来自不同 target"的要求出于"避免只压到一种消费模式"，本任务改用"两个候选压到
两个互相独立的生产文件（`kis_dom_utils.cpp` 与 `kis_distance_information.cpp`，
功能完全不相关：前者是 DOM ↔ 值类型的通用序列化工具，后者是画笔运笔距离追踪
的私有序列化）"来达到近似效果。

### 候选 A：`libs/global/kis_dom_utils.cpp`（Task 3，已交付）

见 §6「Task 3 试接（候选 A）新增/修正」与 `.superpowers/sdd/R-07/task-3-report.md`
完整报告。DOM API 覆盖面广（`createElement`/`ownerDocument`/`appendChild`/
`setAttribute`/`attribute`/`elementsByTagName`/`tagName`/`isElement`），8 个真实
测试方法全部 PASS。

### 候选 B：`libs/image/kis_distance_information.cpp`（本 Task）

`KisDistanceInitInfo::toXML`/`fromXML`（`kis_distance_information.cpp:190-215`）
的 `createElement`/`setAttribute`（隐含）/`firstChildElement` 往返序列化，测试
方法 `testInitInfoXMLClone`（`kis_distance_information_test.cpp:141-168`）四组
不同构造参数的 XML 往返 + `operator==` 断言。

**与线级 plan §2 的一处实测偏离，如实记录**：plan 原文判定候选 B"零额外依赖
……不需要任何 stub，是本任务最干净的试接目标"——这个判断只在"只看 `toXML`/
`fromXML` 两个函数的签名（`QPointF`/`qreal`）"这个口径下成立。实测发现，
`kis_distance_information.cpp` 是**整个翻译单元**编译（C++ 逐 TU 编译，不是逐
函数——与 Task 3 报告"必须作为一个翻译单元编译"的教训一致），文件顶部
`#include`（`<brushengine/kis_paint_information.h>`/`kis_algebra_2d.h`/
`kis_lod_transform.h`）与 `KisDistanceInformation`（另一个类，与
`KisDistanceInitInfo` 同文件但不同类型）的成员函数体一起把
`QVector2D`/`QVector`/`QPolygonF`/`QPainterPath`/`KisPaintInformation`/
`QDebug`/`QList` 等一整批未交付类型拖了进来——比候选 A 的 `QColor`/`i18n` 更深
一层（候选 A 的两个 stub 是纯 Qt/KDE 类型缺口，候选 B 这次还包含一整个 Krita
生产类型 `KisPaintInformation` 的编译期占位）。详细清单见 §9。

`testInitInfoXMLClone` 本身**不触达**任何一处新占位——四组构造参数往返 +
`operator==` 全部只经过 `QPointF`/`qreal`/`bool`/`int` 字段，`operator==`
（`kis_distance_information.cpp:149-170`）逐字段比较也不涉及新占位类型。新占位
覆盖的是**同一 TU 里其它必须编译但不被这个测试用例执行到**的代码路径
（`KisDistanceInformation` 类的插值方法、`kis_lod_transform.h` 的
`KisPaintInformation` 重载）。

### Step 3：跨 target 补充尝试结果——尝试 `kis_mask_generator_test.cpp`，30 分钟
### 时间盒内确认卡在 SIMD/多架构笔刷渲染基础设施上，未跑通

按 plan 优先级选的是 `kis_mask_generator_test.cpp`（"与'笔刷形状'这个真实消费
场景直接相关"）。实测：

1. `libs/image/kis_mask_generator.h` 本身没有独立的 `.cpp`——它只是把 6 个具体
   子类头（`kis_circle_mask_generator.h`/`kis_rect_mask_generator.h`/
   `kis_gauss_circle_mask_generator.h`/`kis_gauss_rect_mask_generator.h`/
   `kis_curve_circle_mask_generator.h`/`kis_curve_rect_mask_generator.h`）汇总
   include。`KisMaskGenerator::fromXML`（测试用到的静态工厂方法）的真身在
   `kis_base_mask_generator.cpp`。
2. `kis_base_mask_generator.cpp` 的 `#include` 列表里有
   `<KoMultiArchBuildSupport.h>`/`kis_brush_mask_applicator_factories.h`/
   `kis_brush_mask_applicator_base.h`/`kis_fast_math.h`——这是笔刷蒙版生成器的
   **SIMD 多架构分派基础设施**（按 CPU 特性在多份向量化实现间选择），与
   `kis_algebra_2d.h`/`KisPaintInformation` 这类"类型缺口，能靠编译期占位绕过"
   的性质不同：多架构分派本身就是这批生成器的核心算法，不是可以随手占位掉
   的旁支依赖。
3. 判断：这不属于"未交付 Qt/KDE 类型缺口"，而是"未交付的 Krita 基础设施子系统
   本身"——按 brief"如果不是真的卡在未交付类型上，说明是 pk/xml 本身的 API
   缺口"的判据类比，这里同样不是"卡在未交付类型"这个可以照候选 A/B 模式加
   stub 绕过的情形，而是这个候选测试目标本身的生产代码依赖了一整个尚未被任何
   R 任务排期的子系统（SIMD 笔刷渲染分派）。30 分钟时间盒内确认卡点后停止，
   未继续深挖，符合 brief"如实记录卡点即可，不用死磕跑通"的降级路径。

**未尝试的另外两个备选**（超出 30 分钟时间盒，未动手，记录判断依据）：

- `sdk/tests/KisDumbTransformMaskParams.cpp`：`#include` 直接拉入
  `kis_node.h`/`kis_painter.h`/`kis_perspectivetransform_worker.h`——完整节点图
  与画笔机制，依赖闭包预期比 `kis_mask_generator_test.cpp` 更大，未验证具体
  卡点。
- `libs/image/tests/kis_paint_information_test.cpp`：生产文件
  `kis_paint_information.cc` 本身就在 `libs/image`（`kritaimage`），与候选 A/B
  同一个 target，即使跑通也不能补上"跨 target"这一条判据；且它的依赖
  （`kis_random_source.h`/`KisPerStrokeRandomSource.h`——`QRandomGenerator`/
  `QMutex`/`QHash`/`boost::random`）与候选 B 撞上的 `KisPaintInformation` 占位
  是同一棵依赖树的更深处，预期同样不是几行占位能绕过的量级。

**结论**：本任务的两个正式候选（A/B）加这一次补充尝试，共同确认"`kritaimage`
之外找不到干净的候选"这条 plan 已经承认的弱点在实测层面成立——不是没有认真
找，是找过的候选无一例外卡在 `QColor`/资源系统/形状系统/SIMD 渲染基础设施
这几类尚未被任何 R 任务排期的缺口上。

## 9. Task 4（候选 B）新增的编译期占位——不是 pk/xml 的交付物

与 Task 3 候选 A 的 `QColor`/`klocalizedstring.h` 同一处置原则（编译期占位，
不是功能替代品，只保证能编过，不构成对这些类型的交付声明），全部放在
`pk/xml/tests/graft/stubs/`：

| 文件 | 处置的缺口 | 是否被 `testInitInfo` 断言真正触达 |
|---|---|---|
| `stubs/QVector2D`（新增） | `kis_distance_information.cpp` 的 `getNextPointPositionIsotropic()`、`kis_spacing_information.h` 的 `scalarApprox()` 用到 `QVector2D(...).length()` | 否——只在 `KisDistanceInformation` 的插值方法里，`testInitInfoXMLClone` 只经过 `KisDistanceInitInfo` |
| `stubs/QVector`（新增） | `kis_algebra_2d.h` 一批自由函数**声明**的形参类型（`polygonDirection`/`sampleRectWithPoints` 等） | 否——只是声明，未被调用 |
| `stubs/QPolygonF`（新增） | 同上（`adjustIfOnPolygonBoundary`/`calculateConvexHull` 等声明） | 否 |
| `stubs/QPainterPath`（新增） | 同上（`VectorPath` 类成员/`smallArrow`/`trySimplifyPath` 等声明） | 否 |
| `stubs/brushengine/kis_paint_information.h` + `stubs/kis_paint_information.h`（新增，转发前者） | **整个 `KisPaintInformation` 类**——`KisDistanceInformation::Private::lastPaintInformation` 值成员、`registerPaintedDab`/`lockCurrentDrawingAngle` 方法体、`kis_lod_transform.h` 的 `map(KisPaintInformation)` 重载、测试文件 `testInterpolation()` 的多参数构造调用 | 否——`testInitInfo` 从不构造 `KisDistanceInformation`（只构造 `KisDistanceInitInfo`），`testInterpolation` 单独是另一个 slot、本试接故意不跑（见 §8） |
| `stubs/QtCore/qmath.h`（新增，真实转发实现，非占位） | `<QtCore/qmath.h>` 与 `-I $BOOST_INC`（`krita-ci-env/_install/include`）共用同一前缀，不抢先命中会解析到真 Qt qglobal.h、与 pk 已有的 `qAbs`/`qRound`/`qMin`/`qMax` 定义冲突 | 部分——`qBound`（`getNextPointPositionTimed`）等仍在未跑的插值路径里，但函数本身是对 `<cmath>` 的真实转发，不是假结果 |
| `stubs/QtMath`（新增） | `kis_distance_information_test.cpp` 的 `#include <QtMath>`，转发到上一行 | 同上 |
| `graft_run_b_driver.cpp` 里的 `KisAlgebra2D::directionBetweenPoints` 占位 | 真身在 `kis_algebra_2d.cpp`（未编译），只声明会链接期报错 | 否——只在 `testInterpolation()` |
| `stubs/QString` 新增 `number(double, char, int)` 重载 | **真正需要工作**——`KisDistanceInitInfo::toXML`/`fromXML` 逐字段调用 `QString::number(qreal, 'g', 15)` 做 XML 属性值序列化 | **是**——`testInitInfoXMLClone` 的四组往返断言直接验证这条路径（`%.*g` 实现，precision 是有效数字位数） |
| `stubs/QtGlobal` 新增 `Q_DECL_HIDDEN` 宏 | `struct Q_DECL_HIDDEN KisDistanceInformation::Private { ... }` pimpl 写法，候选 A 没撞到过 | 编译期宏，不涉及运行时断言 |
| `pk/container/compat/QList`/`pk/log/compat/QDebug` **强制提前 include**（脚本 FORCE 数组，不是新文件） | `kis_algebra_2d.h` 裸用 `QList<T>`/`QDebug`，自己没有 `#include <QList>`/`#include <QDebug>`（复刻真 Qt 经 `<QtGlobal>` 传递可见这条传递性） | 不涉及新文件，只是既有 compat 垫片的强制 include 顺序 |

**一处共用文件的候选 A 回归，已发现并改正**：`KisAlgebra2D::directionBetweenPoints`
最初被加进候选 A/B 共用的 `stubs/graft_stubs.cpp`，导致 `graft_run_a.sh`
（编译 `graft_stubs.cpp` 时不带 FORCE 数组）当场报一片 `QList`/`QDebug` 相关
编译错误——已改为放进候选 B 独有的 `graft_run_b_driver.cpp`，`graft_run_a.sh`
重跑确认恢复绿（10 passed / 0 failed，见「完整命令与输出」）。这是本任务
唯一一次跨候选的回归，记在这里供后续任务参考"共用文件改动前先跑另一个候选
的脚本"这条经验。

## 10. `PkByteArray` 依赖情况回填（Task 1 关键实现说明 5 的待确认项）

Task 1 交付时确认本 worktree**没有**交付 `PkByteArray`（`pk/container/` 下不存在
任何 `*ByteArray*` 文件），`PkXmlDocument::toByteArray()` 退化成 `toString()`
的 UTF-8 编码结果（见 §2.2）。**Task 4 收尾时回填结论**：R-07 全程（Task 1-4）
`pk/container` 状态未变化，`PkByteArray` 依旧未交付，退化路径是唯一实现，
维持不变——这不是本任务的技术债，是 `R-02` 尚未排期到这个类型。若后续
`R-02` 补上 `PkByteArray`，`PkXmlDocument::toByteArray()` 与
`PkXmlStreamWriter` 构造函数（同一个决定，见 §5.1）都要跟着改签名。
