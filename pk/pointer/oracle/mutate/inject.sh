#!/usr/bin/env bash
# pk/pointer 变异测试注入脚本（R-04 Task 4）。
#
# 每条注入把某个非空操作的语义改坏一处，跑对拍/单测确认判据能不能变红，跑完
# 用 `git checkout --` 还原。9 条清单与逐条论证见 task-4-report.md，这里只是
# 让它们可重复执行，不重新论证。
#
# 用法：
#   inject.sh list                列出全部注入编号与一行描述
#   inject.sh apply  <N>          对编号 N 打补丁（不构建、不还原）
#   inject.sh revert <N>          还原编号 N 涉及的文件（等价 git checkout --）
#   inject.sh run    <N>          apply → 跑对拍(+相关单测) → 还原 → 报告结果
#   inject.sh run    all          依次跑完全部 9 条，每条跑完立即还原
#
# **前置条件**：跑之前 `git status --short` 必须是空的——`revert` 只对涉及的
# 文件做 `git checkout --`，它不认得你的其它未提交改动，会一起还原掉。
set -u

HERE="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
POINTER_DIR="$(cd "$HERE/../.." && pwd)"
SHARED_H="$POINTER_DIR/PkSharedPointer.h"
SCOPED_H="$POINTER_DIR/PkScopedPointer.h"

desc() {
    case "$1" in
        1) echo "PkSharedPointer::isNull() 改成 m_p.use_count()==0（判据 A）";;
        2) echo "PkWeakPointer::isNull() 去掉 || m_value==nullptr（判据 A 第二面）";;
        3) echo "PkScopedPointer::reset 去掉自赋值保护 if(m_p==other)return（resetSame）";;
        4) echo "PkScopedPointer 移动构造 =delete 改 =default（判据 C，编译期）";;
        5) echo "PkSharedPointer::operator RestrictedBool 换成 explicit operator bool（判据 B，编译期）";;
        6) echo "2 参构造对 nullptr 短路，不建控制块、不调 deleter（判据 D）";;
        7) echo "dynamicCast 内部换成 static_pointer_cast（应返回 null 时返回非 null）";;
        8) echo "PkScopedArrayPointer 析构 delete[] 换成 delete（数组 vs 标量 delete 不匹配）";;
        9a) echo "reset(T*) 重载：忽略 ptr，等价于 reset()";;
        9b) echo "reset(T*, Deleter) 重载：忽略 deleter，退化成 reset(T*)";;
        *) echo "未知编号: $1" >&2; return 1;;
    esac
}

# 每条注入的 apply/revert 只碰它自己需要的文件，写成 sed 的精确原文匹配——
# 匹配不到就说明源码已经变了，宁可报错也不要在错误的地方打洞。
apply() {
    case "$1" in
        1)
            sed -i 's/bool isNull() const noexcept { return m_p.get() == nullptr; }/bool isNull() const noexcept { return m_p.use_count() == 0; }/' "$SHARED_H"
            ;;
        2)
            sed -i 's/bool isNull() const noexcept { return m_weak.expired() || m_value == nullptr; }/bool isNull() const noexcept { return m_weak.expired(); }/' "$SHARED_H"
            ;;
        3)
            perl -0pi -e 's/    void reset\(T \*other = nullptr\)\n    \{\n        if \(m_p == other\) return;\n        T \*old = m_p;/    void reset(T *other = nullptr)\n    {\n        T *old = m_p;/' "$SCOPED_H"
            ;;
        4)
            sed -i 's/PkScopedPointer(PkScopedPointer &&) = delete;/PkScopedPointer(PkScopedPointer \&\&) = default;/' "$SCOPED_H"
            ;;
        5)
            perl -0pi -e 's/    typedef std::shared_ptr<T> PkSharedPointer::\*RestrictedBool;\n    operator RestrictedBool\(\) const noexcept \{ return isNull\(\) \? nullptr : &PkSharedPointer::m_p; \}/    explicit operator bool() const noexcept { return !isNull(); }/' "$SHARED_H"
            ;;
        6)
            sed -i 's/PkSharedPointer(T \*ptr, Deleter d) : m_p(ptr, d) {}/PkSharedPointer(T *ptr, Deleter d) { if (ptr) m_p = std::shared_ptr<T>(ptr, d); }/' "$SHARED_H"
            ;;
        7)
            sed -i 's/std::dynamic_pointer_cast<X>(m_p)/std::static_pointer_cast<X>(m_p)/' "$SHARED_H"
            ;;
        8)
            sed -i 's/~PkScopedArrayPointer() { delete\[\] m_p; }/~PkScopedArrayPointer() { delete m_p; }/' "$SCOPED_H"
            ;;
        9a)
            sed -i 's/void reset(T \*ptr) { m_p.reset(ptr); }/void reset(T *ptr) { (void)ptr; m_p.reset(); }/' "$SHARED_H"
            ;;
        9b)
            sed -i 's/void reset(T \*ptr, Deleter d) { m_p.reset(ptr, d); }/void reset(T *ptr, Deleter d) { (void)d; m_p.reset(ptr); }/' "$SHARED_H"
            ;;
        *) echo "未知编号: $1" >&2; return 1;;
    esac
}

revert() {
    case "$1" in
        1|2|5|6|7|9a|9b) git -C "$POINTER_DIR" checkout -- "$SHARED_H" ;;
        3|4|8)           git -C "$POINTER_DIR" checkout -- "$SCOPED_H" ;;
        *) echo "未知编号: $1" >&2; return 1;;
    esac
}

run_one() {
    local id="$1"
    echo "=== 注入 #$id: $(desc "$id") ==="
    if ! git -C "$POINTER_DIR" diff --quiet -- "$SHARED_H" "$SCOPED_H"; then
        echo "拒绝执行：$SHARED_H / $SCOPED_H 跑之前就不干净，先处理未提交改动" >&2
        return 1
    fi
    apply "$id" || return 1
    bash "$POINTER_DIR/oracle/run_oracle.sh" > /tmp/inject_oracle.$$.log 2>&1
    local oracle_exit=$?
    grep -E "DIFF total|段错误|已中止|munmap_chunk" /tmp/inject_oracle.$$.log
    echo "oracle exit=$oracle_exit"
    bash "$POINTER_DIR/tests/run_tests.sh" > /tmp/inject_tests.$$.log 2>&1
    local tests_exit=$?
    grep -E "FAIL|Totals:|段错误|错误 [0-9]" /tmp/inject_tests.$$.log
    echo "tests exit=$tests_exit"
    rm -f /tmp/inject_oracle.$$.log /tmp/inject_tests.$$.log
    revert "$id"
    if ! git -C "$POINTER_DIR" diff --quiet -- "$SHARED_H" "$SCOPED_H"; then
        echo "还原失败：$SHARED_H / $SCOPED_H 仍有改动，手工检查" >&2
        return 1
    fi
    echo "=== 注入 #$id 已还原 ==="
    echo
}

case "${1:-}" in
    list)
        for id in 1 2 3 4 5 6 7 8 9a 9b; do printf '%-3s %s\n' "$id" "$(desc "$id")"; done
        ;;
    apply)  apply "${2:?需要编号}" ;;
    revert) revert "${2:?需要编号}" ;;
    run)
        if [ "${2:-}" = "all" ]; then
            for id in 1 2 3 4 5 6 7 8 9a 9b; do run_one "$id" || exit 1; done
        else
            run_one "${2:?需要编号或 all}"
        fi
        ;;
    *)
        echo "用法: $0 {list|apply <N>|revert <N>|run <N>|run all}" >&2
        exit 1
        ;;
esac
