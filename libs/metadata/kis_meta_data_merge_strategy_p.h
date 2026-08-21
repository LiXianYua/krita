/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */
#ifndef KIS_META_DATA_MERGE_STRATEGY_P_H
#define KIS_META_DATA_MERGE_STRATEGY_P_H

#include "kis_meta_data_merge_strategy.h"

class PkString;

namespace KisMetaData
{
class Schema;
class Value;
/**
 * This strategy drop all meta data.
 */
class DropMergeStrategy : public MergeStrategy
{
public:
    DropMergeStrategy();
    ~DropMergeStrategy() override;
    PkString id() const override;
    PkString name() const override;
    PkString description() const override;
    void merge(Store* dst, PkList<const Store*> srcs, PkList<double> score) const override;
};
class PriorityToFirstMergeStrategy : public MergeStrategy
{
public:
    PriorityToFirstMergeStrategy();
    ~PriorityToFirstMergeStrategy() override;
    PkString id() const override;
    PkString name() const override;
    PkString description() const override;
    void merge(Store* dst, PkList<const Store*> srcs, PkList<double> score) const override;
};
class OnlyIdenticalMergeStrategy : public MergeStrategy
{
public:
    OnlyIdenticalMergeStrategy();
    ~OnlyIdenticalMergeStrategy() override;
    PkString id() const override;
    PkString name() const override;
    PkString description() const override;
    void merge(Store* dst, PkList<const Store*> srcs, PkList<double> score) const override;
};
class SmartMergeStrategy : public MergeStrategy
{
public:
    SmartMergeStrategy();
    ~SmartMergeStrategy() override;
    PkString id() const override;
    PkString name() const override;
    PkString description() const override;
    void merge(Store* dst, PkList<const Store*> srcs, PkList<double> score) const override;
protected:
    /**
     * Merge multiple entries in one.
     */
    void mergeEntry(Store* dst, PkList<const Store*> srcs, const Schema* schema, const PkString & identifier) const;
    Value election(PkList<const Store*> srcs, PkList<double> score, const PkString & key) const;
};
}
#endif
