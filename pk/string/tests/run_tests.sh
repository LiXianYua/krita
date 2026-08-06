#!/usr/bin/env bash
# 跑 test_pkstring —— 但在跑之前先尽量给它备一个「小数点是逗号」的 locale。
#
# 为什么需要这一层：test_format.cpp 里 LC_NUMERIC 免疫那一组是整套测试中最强的
# 一项检查（toDouble / arg(double) 必须像 QString 一样硬编码 C locale），可它只有
# 在系统装了 de_DE 之类的 locale 时才跑得起来。绝大多数开发机与 CI 只装了 C 和本地
# 语言两个 locale，于是那一组会**静默不跑**，而 replacement.sh ⑨ 把测试输出丢进
# /dev/null，连那行 NOTE 都看不见 —— 判据指着一个不存在的东西时不会报错，只会放行。
#
# localedef 生成 locale **不需要 sudo**：输出到构建目录、用 LOCPATH 指过去即可。
# 生成不了（没有 localedef、或没装 i18n 源文件）也不让测试失败——那种环境下
# 那一组检查确实跑不了，测试会自己打印 NOTE 说明。
set -u
cd "$(dirname "$0")/../../.." || exit 1     # → fork 仓库根

BIN=pk/string/build/test_pkstring
if [ ! -x "$BIN" ]; then
    printf 'test binary not built: %s\n' "$BIN"
    exit 1
fi

LOCDIR=pk/string/build/locale
if [ ! -d "$LOCDIR/de_DE.UTF-8" ] \
   && command -v localedef >/dev/null 2>&1 \
   && [ -f /usr/share/i18n/locales/de_DE ]; then
    mkdir -p "$LOCDIR/de_DE.UTF-8"
    localedef -i de_DE -f UTF-8 "$LOCDIR/de_DE.UTF-8" >/dev/null 2>&1 \
        || rm -rf "$LOCDIR/de_DE.UTF-8"
fi
if [ -d "$LOCDIR/de_DE.UTF-8" ]; then
    LOCPATH=$(cd "$LOCDIR" && pwd)
    export LOCPATH
fi

# 不能 exec：跑完还要判断「LC_NUMERIC 免疫那一组到底跑没跑」。
out=$("./$BIN" 2>&1)
rc=$?
printf '%s\n' "$out"
if [ "$rc" -ne 0 ]; then
    exit "$rc"
fi

# 测试全绿 ≠ 判据够强。上面那一组没跑到时测试也会绿，而 replacement.sh ⑨ 把
# stdout 和 stderr 都重定向进 /dev/null —— 连测试自己打印的 NOTE 都看不见。
# 所以这里把「最强的一组没跑到」升格成非 0 退出：偏离可以，无声无息不行。
# 确实无法造 locale 的环境（没 localedef / 没 i18n 源）可以显式
# PKSTRING_ALLOW_NO_LOCALE=1 放行 —— 那是一个要有人主动做的决定。
if printf '%s' "$out" | grep -q 'LC_NUMERIC 免疫检查未跑到'; then
    if [ "${PKSTRING_ALLOW_NO_LOCALE:-0}" = "1" ]; then
        printf 'run_tests.sh: LC_NUMERIC 免疫检查未跑到，已按 PKSTRING_ALLOW_NO_LOCALE=1 放行\n' >&2
    else
        printf 'run_tests.sh: LC_NUMERIC 免疫检查未跑到，判为失败。\n' >&2
        printf '  装 localedef 与 /usr/share/i18n/locales/de_DE 后重跑，或显式设\n' >&2
        printf '  PKSTRING_ALLOW_NO_LOCALE=1 承认这一轮没验到这一项。\n' >&2
        exit 1
    fi
fi
exit 0
