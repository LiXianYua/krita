#!/usr/bin/env python3
"""替代 moc 做测试发现。

moc 在 Krita 的测试里只干一件事：让 QTest::qExec 能按名字枚举 private slots。
本脚本做同一件事，产物是一个 PkTestBinder<T> 特化 —— 编译期的函数表，
没有元对象、没有运行时反射。纯文本扫描，不解析完整 C++ 语法。

用法：
    pk_test_moc.py <input.h> -o <output.cpp> [--stats]

--stats 额外往 stdout 打一行 `<input>\t<classCount>\t<testFuncCount>`，
供全量扫描自证统计用；不影响 -o 产物。
"""

import argparse
import os
import re
import sys

# 分类进 fixture 槽的槽名（照 QTest 语义，精确匹配，不含其余 *_data 变体）。
FIXTURES = {"initTestCase", "initTestCase_data", "cleanupTestCase", "init", "cleanup"}

# fixture 槽名 → PkTestBinder<T> 对应的静态成员名。
_FIXTURE_ACCESSORS = (
    ("initTestCase", "initTestCase"),
    ("cleanupTestCase", "cleanupTestCase"),
    ("init", "initFn"),
    ("cleanup", "cleanupFn"),
    ("initTestCase_data", "initTestCaseData"),
)

# 注释剥离：先干掉 /* */ 与 //，避免注释里的 `void foo();` 被误收。
_BLOCK_COMMENT = re.compile(r"/\*.*?\*/", re.S)
_LINE_COMMENT = re.compile(r"//[^\n]*")

_CLASS_HEAD = re.compile(r"\bclass\s+(?:\w+_EXPORT\s+)?(\w+)\s*(?::[^{]*)?\{")

# 访问区边界。真实 Krita 代码里 Q_SIGNALS:/signals: 经常裸写、不带
# public/protected/private 前缀（跟在 private Q_SLOTS: 段后面直接出现）；
# 两个捕获组分别对应"带访问关键字"和"裸写关键字"两条分支，取其一为 kind。
# 两条分支都要求至少出现 public/protected/private/Q_SLOTS/Q_SIGNALS/slots/signals
# 之一——不能让访问关键字与 kind 全部可选，那样会退化成匹配任意冒号
# （三目表达式、初始化列表、位域全中招）。
_ACCESS = re.compile(
    r"\b(?:(?:public|protected|private)\s*(Q_SLOTS|Q_SIGNALS|slots|signals)?"
    r"|(Q_SLOTS|Q_SIGNALS|slots|signals))\s*:"
)
_FUNC = re.compile(r"\bvoid\s+(\w+)\s*\(\s*\)\s*;")


def strip_comments(text):
    return _LINE_COMMENT.sub("", _BLOCK_COMMENT.sub("", text))


def class_body(text, open_brace_index):
    """从 `{` 起按花括号配平找类体，返回 (body, end_index)。

    不能靠"第一个 };"——嵌套类 / 嵌套 {} 会把它切在错的地方。
    """
    depth = 0
    i = open_brace_index
    while i < len(text):
        if text[i] == "{":
            depth += 1
        elif text[i] == "}":
            depth -= 1
            if depth == 0:
                return text[open_brace_index + 1:i], i
        i += 1
    return None, len(text)


def collect_slot_functions(body):
    """只取 `private Q_SLOTS:`（含其余 *_SLOTS 变体）段里的无参 void 函数。

    段之间用访问区边界切分：Q_SLOTS:/slots: 开块；Q_SIGNALS:/signals:/
    public:/protected:/private: 关块——`private Q_SLOTS:` 之后再出现其中
    任何一种，都视为块结束，其后的函数不算测试。信号不是槽，就算它紧跟在
    private Q_SLOTS: 后面裸写也不算数。
    """
    names = []
    in_slots = False
    last = 0
    for m in _ACCESS.finditer(body):
        segment = body[last:m.start()]
        if in_slots:
            names.extend(_FUNC.findall(segment))
        kind = m.group(1) or m.group(2)
        in_slots = kind in ("Q_SLOTS", "slots")
        last = m.end()
    if in_slots:
        names.extend(_FUNC.findall(body[last:]))
    return names


def classify(class_name, slot_names):
    """把槽名分成 fixture / 数据函数 / 测试函数三类。

    分类规则：精确匹配 FIXTURES → fixture；名字以 _data 结尾 → 数据函数，
    且把去掉 _data 的同名测试函数的 dataName 指向它；其余 → 测试函数。
    """
    fixtures = set()
    data_names = []
    seen_data = set()
    test_names = []
    for name in slot_names:
        if name in FIXTURES:
            fixtures.add(name)
        elif name.endswith("_data"):
            if name not in seen_data:
                seen_data.add(name)
                data_names.append(name)
        else:
            test_names.append(name)

    functions = []
    for name in test_names:
        pair = name + "_data"
        functions.append((name, pair if pair in seen_data else None))

    return {
        "className": class_name,
        "functions": functions,
        "dataFunctions": data_names,
        "fixtures": fixtures,
    }


def discover(text):
    """返回 [{className, functions, dataFunctions, fixtures}, ...]。

    只收有 Q_OBJECT 且 private Q_SLOTS: 段非空的类；没有 Q_OBJECT 的类
    （例如普通基类）直接跳过，不生成 PkTestBinder 特化。
    """
    text = strip_comments(text)
    results = []
    pos = 0
    while True:
        m = _CLASS_HEAD.search(text, pos)
        if not m:
            break
        name = m.group(1)
        body, end = class_body(text, m.end() - 1)
        pos = end + 1
        if body is None or "Q_OBJECT" not in body:
            continue
        slot_names = collect_slot_functions(body)
        if not slot_names:
            continue
        results.append(classify(name, slot_names))
    return results


def _emit_list_accessor(class_name, accessor, count_accessor, entries):
    """emit functions()/dataFunctions() 与对应的 count()/dataCount()。

    entries 为空时返回 nullptr/0——C++ 里零长数组不合法，不能写
    `static const PkTestFunction fns[] = {};`。
    """
    if not entries:
        return [
            f"    static const PkTestFunction *{accessor}() {{ return nullptr; }}",
            f"    static int {count_accessor}() {{ return 0; }}",
        ]
    lines = [f"    static const PkTestFunction *{accessor}() {{",
             "        static const PkTestFunction fns[] = {"]
    for fn_name, data_name in entries:
        data_lit = f'"{data_name}"' if data_name else "nullptr"
        lines.append(
            f'            {{"{fn_name}", '
            f"[](PkTestObject *o){{ static_cast<{class_name} *>(o)->{fn_name}(); }}, "
            f"{data_lit}}},"
        )
    lines.append("        };")
    lines.append("        return fns;")
    lines.append("    }")
    lines.append(f"    static int {count_accessor}() {{ return {len(entries)}; }}")
    return lines


def _emit_single_accessor(class_name, accessor, fn_name):
    """emit 五个 fixture 槽里的一个：initTestCase()/cleanupTestCase()/...。"""
    if not fn_name:
        return [f"    static const PkTestFunction *{accessor}() {{ return nullptr; }}"]
    return [
        f"    static const PkTestFunction *{accessor}() {{",
        f'        static const PkTestFunction fn{{"{fn_name}", '
        f"[](PkTestObject *o){{ static_cast<{class_name} *>(o)->{fn_name}(); }}, nullptr}};",
        "        return &fn;",
        "    }",
    ]


def emit_binder(info):
    name = info["className"]
    lines = [f"template <> struct PkTestBinder<{name}> {{",
             f'    static const char *className() {{ return "{name}"; }}']
    lines += _emit_list_accessor(name, "functions", "count", info["functions"])
    lines += _emit_list_accessor(
        name, "dataFunctions", "dataCount",
        [(n, None) for n in info["dataFunctions"]])
    for slot_name, accessor in _FIXTURE_ACCESSORS:
        fn_name = slot_name if slot_name in info["fixtures"] else None
        lines += _emit_single_accessor(name, accessor, fn_name)
    lines.append("};")
    return "\n".join(lines)


def emit(header_path, classes):
    header_abs = os.path.abspath(header_path)
    parts = [
        f"// 由 pk/test/pk_test_moc.py 从 {header_path} 生成。不要手改。",
        f'#include "{header_abs}"',
        '#include "PkTest.h"',
        "",
    ]
    parts += [emit_binder(info) for info in classes]
    return "\n".join(parts) + "\n"


def main():
    parser = argparse.ArgumentParser(description=__doc__,
                                      formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("input", help="要扫描的测试头（.h）")
    parser.add_argument("-o", "--output", required=True, help="生成的 binder .cpp 路径")
    parser.add_argument("--stats", action="store_true",
                         help="额外往 stdout 打一行 <input>\\t<classCount>\\t<testFuncCount>")
    args = parser.parse_args()

    with open(args.input, "r", encoding="utf-8", errors="ignore") as f:
        text = f.read()

    classes = discover(text)

    with open(args.output, "w", encoding="utf-8") as f:
        f.write(emit(args.input, classes))

    if args.stats:
        func_count = sum(len(c["functions"]) for c in classes)
        print(f"{args.input}\t{len(classes)}\t{func_count}")

    return 0


if __name__ == "__main__":
    sys.exit(main())
