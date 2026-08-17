#!/usr/bin/env python3
"""pk_signal_moc.py 生成器行为自测。

钉死三件评审发现的行为，纯 Python、零编译依赖（不碰 QList 等真实 Krita 类型）：

1. split_top_level 对嵌套模板参数——内层逗号（QHash<int,int>）与尾 >>（
   QList<QVector<int>>）不能被当成分隔符或误切；两个顶层逗号才是参数边界。
2. 无名字 cv-qualified 参数（const char * const / const int）不许把尾 const/内建
   标量关键字误判成参数名，要回退为「无名字、补名 arg{idx}」。
3. 默认参数（顶层 =）必须 fail-fast（SystemExit 非零退出），不能静默产出非法 C++。

用法：python3 pk/signal/tests/selftest_generator.py（在 fork 仓库根跑，或任意目录，
脚本自身按 __file__ 定位 pk_signal_moc.py 与 generator_cases 输入）。
"""
import os
import subprocess
import sys

HERE = os.path.dirname(os.path.abspath(__file__))
SIGNAL_DIR = os.path.abspath(os.path.join(HERE, ".."))
sys.path.insert(0, SIGNAL_DIR)

import pk_signal_moc as moc

CASES = os.path.join(HERE, "generator_cases")

g_failures = 0


def check(cond, label, extra=""):
    global g_failures
    if cond:
        return
    g_failures += 1
    print(f"FAIL: {label}  {extra}")


def check_eq(got, want, label):
    check(got == want, label, f"(got {got!r}, want {want!r})")


def test_split_top_level():
    s = "const QList<QVector<int>>& items, const QHash<int,int>& map"
    parts = moc.split_top_level(s)
    check_eq(parts, ["const QList<QVector<int>>& items",
                     " const QHash<int,int>& map"],
             "split_top_level 把两个顶层参数切开、不切内层逗号")

    s2 = "const QPair<QVector<int>, QHash<int,int>>& pair"
    check_eq(moc.split_top_level(s2), [s2],
             "split_top_level 对单参数（内层含逗号）切出 1 段")

    s3 = "a, b<c>, d<e,f>, g"
    check_eq(moc.split_top_level(s3), ["a", " b<c>", " d<e,f>", " g"],
             "split_top_level 只按顶层逗号切")


def test_parse_cv_unnamed():
    # const char * const：尾 const 是 cv 限定符，不是参数名 → 无名字，补 arg0
    name, types, names = moc.parse_signal_decl(
        "void unnamedPtr(const char * const);", "C")
    check_eq(name, "unnamedPtr", "unnamedPtr 信号名")
    check_eq(types, ["const char * const"], "cv 尾 const 归为类型")
    check_eq(names, ["arg0"], "cv 尾 const 判为无名字、补 arg0")

    # const int：int 是内建标量类型，不是名字 → 无名字
    name, types, names = moc.parse_signal_decl(
        "void unnamedInt(const int);", "C")
    check_eq(types, ["const int"], "const int 全部作为类型")
    check_eq(names, ["arg0"], "const int 判为无名字、补 arg0")

    # volatile 同样被误抓为名字的 cv 关键字
    name, types, names = moc.parse_signal_decl(
        "void unnamedVol(const char * volatile);", "C")
    check_eq(types, ["const char * volatile"], "尾 volatile 归为类型")
    check_eq(names, ["arg0"], "尾 volatile 判为无名字")


def test_parse_named_still_works():
    name, types, names = moc.parse_signal_decl(
        "void withArgs(const char* s, int n);", "C")
    check_eq(types, ["const char*", "int"], "带名参数类型")
    check_eq(names, ["s", "n"], "带名参数名")


def test_default_arg_fail_fast():
    try:
        moc.parse_signal_decl("void defarg(int a = 0);", "C")
        check(False, "默认参数应抛 SystemExit", "但没抛")
    except SystemExit as e:
        msg = str(e)
        check("default argument not supported" in msg and "defarg" in msg,
              "默认参数报错信息", f"(got {msg!r})")


def test_discover_nested_template_header():
    path = os.path.join(CASES, "nested_template_only.h")
    with open(path, "r", encoding="utf-8") as f:
        classes = moc.discover(f.read())
    check_eq(len(classes), 1, "nested_template_only.h 发现 1 个类")
    c = classes[0]
    check_eq(c["className"], "NestedSender", "类名 NestedSender")
    check_eq(len(c["signals"]), 2, "NestedSender 有 2 个信号")

    name, types, names = moc.parse_signal_decl(
        [d for d in c["signals"] if "nestedTemplate" in d][0], c["className"])
    check_eq(types,
             ["const QList<QVector<int>>&", "const QHash<int,int>&"],
             "nestedTemplate 类型切分正确（内层逗号不被误切）")
    check_eq(names, ["items", "map"], "nestedTemplate 参数名")

    name, types, names = moc.parse_signal_decl(
        [d for d in c["signals"] if "nestedVector" in d][0], c["className"])
    check_eq(types, ["const QPair<QVector<int>, QHash<int,int>>&"],
             "nestedVector 类型切分正确")
    check_eq(names, ["pair"], "nestedVector 参数名")


def test_end_to_end_nested_template():
    path = os.path.join(CASES, "nested_template_only.h")
    subprocess.run(
        [sys.executable, os.path.join(SIGNAL_DIR, "pk_signal_moc.py"),
         path, "-o", "/dev/null"],
        check=True, stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    check(True, "生成器端到端处理 nested_template_only.h 不崩（exit 0）")


def test_end_to_end_fail_fast():
    # 写临时默认参数输入，断言生成器非零退出 + stderr 含 default argument。
    import tempfile
    with tempfile.NamedTemporaryFile("w", suffix=".h", delete=False) as f:
        f.write("class D : public PkObject {\npublic:\nQ_SIGNALS:\n"
                "    void sig(int a = 0);\n};\n")
        tmp = f.name
    try:
        r = subprocess.run(
            [sys.executable, os.path.join(SIGNAL_DIR, "pk_signal_moc.py"),
             tmp, "-o", "/dev/null"],
            capture_output=True, text=True)
        check(r.returncode != 0, "默认参数生成器应非零退出",
              f"(exit {r.returncode})")
        check("default argument" in r.stderr, "stderr 含 default argument",
              f"(stderr {r.stderr!r})")
    finally:
        os.unlink(tmp)


def main():
    test_split_top_level()
    test_parse_cv_unnamed()
    test_parse_named_still_works()
    test_default_arg_fail_fast()
    test_discover_nested_template_header()
    test_end_to_end_nested_template()
    test_end_to_end_fail_fast()

    if g_failures:
        print(f"{g_failures} generator selftest checks failed")
        return 1
    print("generator selftest OK")
    return 0


if __name__ == "__main__":
    sys.exit(main())