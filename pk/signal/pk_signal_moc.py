#!/usr/bin/env python3
"""替代 moc 做信号定义生成。

moc 对信号干的事：把 `Q_SIGNALS:` 段里的 `void sig(...);` 声明生成一个定义，
定义体调用元对象系统的 activate。本脚本做同一件事，产物是「信号成员函数的定义
+ 调用 PkObject::activateSignal」，没有元对象、没有字符串表。

与 pk/test/pk_test_moc.py 的分工：那个生成测试发现 binder（private Q_SLOTS），
这个生成信号定义（Q_SIGNALS）。两者各自独立工程、各自拥有，不复用同一个脚本
（test 扫槽、signal 扫信号，职责不同，且锁不同）。

用法：
    pk_signal_moc.py <input.h> -o <output.cpp>

对解析不了的信号声明（嵌套模板里带逗号、默认参数等），**报错退出**而不是静默
生成错的——信号定义错了是链接期/运行期错，报错点离真相越近越好。
"""
import argparse
import os
import re
import sys

# 剥注释：与 pk_test_moc.py 同算法（单趟扫描，// 到行尾、/* 到 */，块注释按行数换行）。
def strip_comments(text):
    out = []
    i = 0
    n = len(text)
    while i < n:
        if text[i] == "/" and i + 1 < n:
            nxt = text[i + 1]
            if nxt == "/":
                end = text.find("\n", i)
                i = n if end < 0 else end
                continue
            if nxt == "*":
                end = text.find("*/", i + 2)
                block = text[i:] if end < 0 else text[i:end + 2]
                out.append("\n" * block.count("\n"))
                i = n if end < 0 else end + 2
                continue
        out.append(text[i])
        i += 1
    return "".join(out)

_CLASS_HEAD = re.compile(r"\bclass\s+(?:\w+_EXPORT\s+)?(\w+)\s*(?::[^{]*)?\{")
_ACCESS = re.compile(
    r"\b(?:(?:public|protected|private)\s*(Q_SLOTS|Q_SIGNALS|slots|signals)?"
    r"|(Q_SLOTS|Q_SIGNALS|slots|signals))\s*:")

def class_body(text, open_brace_index):
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

def split_top_level(s, sep=","):
    """按顶层逗号切分（不切括号/尖括号内的逗号）。"""
    parts, depth, cur = [], 0, []
    for ch in s:
        if ch in "([{<":
            depth += 1
        elif ch in ")]}>":
            depth -= 1
        if ch == sep and depth == 0:
            parts.append("".join(cur)); cur = []
        else:
            cur.append(ch)
    if cur:
        parts.append("".join(cur))
    return parts

_PARAM_NAME = re.compile(r"([A-Za-z_]\w*)\s*$")

# C++ 关键字中「会作为类型的最后一个词出现、因而被 _PARAM_NAME 误抓成参数名」的集合：
# cv 限定符（`const char * const` 的尾 `const`）+ 内建标量类型（`const int` 的尾 `int`）。
# 合法 C++ 里参数名不可能叫这些词，把它们判为「非名字」零误伤。
_TRAILING_TYPE_KEYWORDS = {
    "const", "volatile",
    "signed", "unsigned",
    "int", "char", "short", "long", "float", "double", "bool",
    "wchar_t", "char8_t", "char16_t", "char32_t", "void", "auto",
}

def _has_top_level(s, ch):
    """扫描 s，看目标字符是否出现在括号/尖括号/花括号之外（顶层）。"""
    depth = 0
    for c in s:
        if c in "([{<":
            depth += 1
        elif c in ")]}>":
            depth -= 1
        if c == ch and depth == 0:
            return True
    return False

def parse_signal_decl(decl, class_name):
    """decl 形如 `void sigTest2(const QString &arg1, const QString &arg2);`。
    返回 (name, [param_type_text...], [param_name...])。"""
    m = re.match(r"\s*void\s+(\w+)\s*\((.*)\)\s*;?\s*$", decl, re.S)
    if not m:
        raise SystemExit(f"pk_signal_moc: not a void signal decl in {class_name}: {decl!r}")
    name = m.group(1)
    raw_params = split_top_level(m.group(2))
    types, names = [], []
    for idx, p in enumerate(raw_params):
        p = p.strip()
        if not p:
            continue  # 无参
        # 默认参数（顶层 =）：本脚本不生成它，fail-fast 报错而不是产出非法 C++。
        # 信号声明里不会有 operator=，顶层 = 即默认参数。
        if _has_top_level(p, "="):
            raise SystemExit(
                f"pk_signal_moc: default argument not supported in {class_name}::sig: {decl!r}")
        # 参数名 = 末尾标识符，但「类型」必须以 非标识符 结尾才是真名字。
        # `const QString &arg1` → 名字 arg1；`void *cookie` → cookie；`qreal`（无名字）→ 无。
        nm = _PARAM_NAME.search(p)
        if nm and nm.group(1) not in _TRAILING_TYPE_KEYWORDS:
            # 名字前的字符必须是 & * 空白（`&arg1`/`*cookie`/` arg1`），
            # 不能是 :: > ] 等（那说明是 `Foo::Bar`/`Vec<int>`/`arr[0]` 的类型尾）。
            before = p[:nm.start()]
            if before and before[-1] in "&* \t":
                names.append(nm.group(1))
                types.append(before.rstrip())
                continue
        names.append(f"arg{idx}")
        types.append(p)
    return name, types, names

def collect_signals(body):
    """把 Q_SIGNALS/signals 段里的声明抓出来。段边界与 pk_test_moc.py 反向：
    signals 开块，Q_SLOTS/slots/public/protected/private 关块。"""
    decls = []
    in_sig = False
    last = 0
    for m in _ACCESS.finditer(body):
        seg = body[last:m.start()]
        if in_sig:
            decls.extend(extract_void_decls(seg))
        kind = m.group(1) or m.group(2)
        in_sig = kind in ("Q_SIGNALS", "signals")
        last = m.end()
    if in_sig:
        decls.extend(extract_void_decls(body[last:]))
    return decls

def extract_void_decls(segment):
    """抓 `void name(...);` 声明。信号声明可能在注释/别的成员函数之间，
    用「void 开头 + 括号配对 + 分号结尾」的扫描抓完整声明（参数可能跨行）。"""
    out = []
    i = 0
    while True:
        m = re.search(r"\bvoid\s+[A-Za-z_]\w*\s*\(", segment[i:])
        if not m:
            break
        start = i + m.start()
        # 括号配对找右括号
        depth = 0
        j = start + segment[start:].find("(")
        while j < len(segment):
            if segment[j] == "(": depth += 1
            elif segment[j] == ")":
                depth -= 1
                if depth == 0: break
            j += 1
        if depth != 0:
            raise SystemExit(f"pk_signal_moc: unbalanced parens at {segment[start:start+40]!r}")
        semi = segment.find(";", j)
        if semi < 0:
            raise SystemExit(f"pk_signal_moc: no ; after signal at {segment[start:start+40]!r}")
        out.append(segment[start:semi + 1])
        i = semi + 1
    return out

def discover(text):
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
        if body is None:
            continue
        sigs = collect_signals(body)
        if sigs:
            results.append({"className": name, "signals": sigs})
    return results

def emit(header_path, classes):
    # 头文件用绝对路径 include（同 pk_test_moc.py）：生成产物可能被任意 TU 从
    # 任意 include 根下 #include，相对路径会随产物位置漂，绝对路径让产物自包含。
    header_abs = os.path.abspath(header_path)
    lines = [
        f"// 由 pk/signal/pk_signal_moc.py 从 {header_path} 生成。不要手改。",
        f'#include "{header_abs}"',
        '#include "PkObject.h"',
        "",
    ]
    for c in classes:
        for decl in c["signals"]:
            name, types, names = parse_signal_decl(decl, c["className"])
            params = ", ".join(f"{t} {n}" for t, n in zip(types, names))
            tmpl = ", ".join(types) if types else ""
            fwd = ", ".join(names)
            # 重载消歧：同名信号有多组参数时 `&C::name` 是模糊的，必须 static_cast
            # 到精确签名 void (C::*)(types...)。非重载信号同样成立，统一用它。
            cast = (f"static_cast<void ({c['className']}::*)({tmpl})>"
                    f"(&{c['className']}::{name})")
            args = (", " + fwd) if fwd else ""
            lines.append(f"void {c['className']}::{name}({params})")
            lines.append("{")
            lines.append(
                f"    PkObject::activateSignal<{tmpl}>("
                f"this, PkMemberFnKey::from({cast}){args});")
            lines.append("}")
            lines.append("")
    return "\n".join(lines) + "\n"

def main():
    p = argparse.ArgumentParser(description=__doc__)
    p.add_argument("input")
    p.add_argument("-o", "--output", required=True)
    args = p.parse_args()
    with open(args.input, "r", encoding="utf-8", errors="ignore") as f:
        text = f.read()
    with open(args.output, "w", encoding="utf-8") as f:
        f.write(emit(args.input, discover(text)))
    return 0

if __name__ == "__main__":
    sys.exit(main())
