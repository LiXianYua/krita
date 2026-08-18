// pk/config/tests/graft/probe/kis_cumulative_undo_data_probe.inc
// 挂接给 graft_run.sh 生成的 driver.cpp：构造一个 PkConfigGroup，用真实、未修改的
// KisCumulativeUndoData::write()/read() 做一次往返，核对值与默认值回退两条路径。
//
// 与 libs/command/KisCumulativeUndoData.h/.cpp 的真实签名核对过（2026-08-17）：
//   read(const KConfigGroup *config)  -> bool
//   write(KConfigGroup *config) const -> void
//   4 个 int 字段：excludeFromMerge / mergeTimeout / maxGroupSeparation / maxGroupDuration
//   operator==（boost::equality_comparable<KisCumulativeUndoData> + 头内 friend 定义）
//   static const KisCumulativeUndoData defaultValue;
// 与 KConfigGroup = PkConfigGroup 垫片核对过：
//   readEntry(const PkString&, int) -> int   writeEntry(const PkString&, int) -> void
//   PkSharedConfig::openConfig() -> PkSharedConfig*，.group(const PkString&) -> PkConfigGroup（按值）
#include <cstdio>
#include <cstdlib>

int main()
{
    // ① 默认值回退：全新 group，read() 应该拿到 KisCumulativeUndoData::defaultValue 的各字段
    {
        PkConfigGroup g = PkSharedConfig::openConfig()->group("probe_defaults");
        KisCumulativeUndoData data;
        data.excludeFromMerge = -1;   // 故意先污染，确认 read() 真的重写了它
        bool ok = data.read(&g);
        if (!ok) { std::fprintf(stderr, "probe: read() returned false\n"); return 1; }
        if (data.excludeFromMerge != KisCumulativeUndoData::defaultValue.excludeFromMerge ||
            data.mergeTimeout != KisCumulativeUndoData::defaultValue.mergeTimeout ||
            data.maxGroupSeparation != KisCumulativeUndoData::defaultValue.maxGroupSeparation ||
            data.maxGroupDuration != KisCumulativeUndoData::defaultValue.maxGroupDuration) {
            std::fprintf(stderr, "probe: default fallback mismatch\n");
            return 1;
        }
    }

    // ② 往返：write() 一组非默认值，另一个 KisCumulativeUndoData 实例 read() 回来必须match
    {
        PkConfigGroup g = PkSharedConfig::openConfig()->group("probe_roundtrip");
        KisCumulativeUndoData written;
        written.excludeFromMerge = 7;
        written.mergeTimeout = 1234;
        written.maxGroupSeparation = 999;
        written.maxGroupDuration = 8888;
        written.write(&g);

        KisCumulativeUndoData readBack;
        bool ok = readBack.read(&g);
        if (!ok) { std::fprintf(stderr, "probe: read() returned false (roundtrip)\n"); return 1; }
        if (!(readBack == written)) {
            std::fprintf(stderr, "probe: roundtrip mismatch\n");
            return 1;
        }
    }

    std::printf("PASS: KisCumulativeUndoData read/write roundtrip via PkConfigGroup\n");
    return 0;
}
