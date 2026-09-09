#pragma once

#include "PkString.h"

#include <filesystem>
#include <map>
#include <mutex>
#include <vector>

// Process-wide configuration storage. Values remain opaque strings here;
// PkConfigGroup owns the typed codecs. Mutations are journaled so sync() can
// merge them with changes made by another process without replacing that
// process's unrelated keys.
class PkConfigStore
{
public:
    static PkConfigStore &instance();

#ifdef PKCONFIG_ENABLE_TEST_HOOKS
    enum class CommitFailureForTesting {
        None,
        ParentOpen,
        ParentFsync
    };

    // Tests inject an exact file before the singleton is constructed. This is
    // deliberately independent of every platform's environment conventions.
    static bool setConfigFilePathForTesting(const std::filesystem::path &path);
    static std::filesystem::path configFilePathForTesting();
    static std::filesystem::path defaultConfigFilePathForTesting();
    static std::filesystem::path defaultConfigLockFilePathForTesting();
    static void setCommitFailureForTesting(CommitFailureForTesting failure);
#endif

    PkString get(const PkString &group, const PkString &key, const PkString &fallback) const;
    void set(const PkString &group, const PkString &key, const PkString &value);
    bool has(const PkString &group, const PkString &key) const;
    void remove(const PkString &group, const PkString &key);
    void clearGroup(const PkString &group);

    // Atomically merges pending mutations into the shared kritarc. Failure
    // leaves both the previous file and the pending in-memory mutations intact
    // so a later call can retry.
    bool sync() noexcept;

private:
    using Group = std::map<PkString, PkString>;
    using Data = std::map<PkString, Group>;

    enum class MutationKind {
        Set,
        Remove,
        ClearGroup
    };

    struct Mutation {
        MutationKind kind;
        PkString group;
        PkString key;
        PkString value;
    };

    PkConfigStore();
    ~PkConfigStore();
    PkConfigStore(const PkConfigStore &) = delete;
    PkConfigStore &operator=(const PkConfigStore &) = delete;

    mutable std::mutex m_mutex;
    Data m_data;
    std::vector<Mutation> m_pending;
    std::filesystem::path m_configPath;
    bool m_persistentStateValid = true;
};
