/*
 *  SPDX-FileCopyrightText: 2008 Cyrille Berger <cberger@cberger.net>
 *
 *  SPDX-License-Identifier: LGPL-2.1-or-later
 */

#include "kis_meta_data_merge_strategy_p.h"

#include <PkString.h>
#include <PkVariant.h>
#include <map>

#include "kis_debug.h"

#include "kis_meta_data_entry.h"
#include "kis_meta_data_schema.h"
#include "kis_meta_data_schema_registry.h"
#include "kis_meta_data_store.h"
#include "kis_meta_data_value.h"

using namespace KisMetaData;

//-------------------------------------------//
//------------ DropMergeStrategy ------------//
//-------------------------------------------//

DropMergeStrategy::DropMergeStrategy()
{
}

DropMergeStrategy::~DropMergeStrategy()
{
}

PkString DropMergeStrategy::id() const
{
    return "Drop";
}
PkString DropMergeStrategy::name() const
{
    return PkString("Drop");
}

PkString DropMergeStrategy::description() const
{
    return PkString("Drop all meta data");
}

void DropMergeStrategy::merge(Store* dst, PkList<const Store*> srcs, PkList<double> score) const
{
    Q_UNUSED(dst);
    Q_UNUSED(srcs);
    Q_UNUSED(score);
    dbgMetaData << "Drop meta data";
}

//---------------------------------------//
//---------- DropMergeStrategy ----------//
//---------------------------------------//

PriorityToFirstMergeStrategy::PriorityToFirstMergeStrategy()
{
}

PriorityToFirstMergeStrategy::~PriorityToFirstMergeStrategy()
{
}

PkString PriorityToFirstMergeStrategy::id() const
{
    return "PriorityToFirst";
}
PkString PriorityToFirstMergeStrategy::name() const
{
    return PkString("Priority to first meta data");
}

PkString PriorityToFirstMergeStrategy::description() const
{
    return PkString("Use in priority the meta data from the layers at the bottom of the stack.");
}

void PriorityToFirstMergeStrategy::merge(Store* dst, PkList<const Store*> srcs, PkList<double> score) const
{
    Q_UNUSED(score);
    dbgMetaData << "Priority to first meta data";

    for (const Store* store : srcs) {
        PkList<PkString> keys = store->keys();
        for (const PkString & key : keys) {
            if (!dst->containsEntry(key)) {
                dst->addEntry(store->getEntry(key));
            }
        }
    }
}
//-------------------------------------------//
//------ OnlyIdenticalMergeStrategy ---------//
//-------------------------------------------//

OnlyIdenticalMergeStrategy::OnlyIdenticalMergeStrategy()
{
}

OnlyIdenticalMergeStrategy::~OnlyIdenticalMergeStrategy()
{
}

PkString OnlyIdenticalMergeStrategy::id() const
{
    return "OnlyIdentical";
}
PkString OnlyIdenticalMergeStrategy::name() const
{
    return PkString("Only identical");
}

PkString OnlyIdenticalMergeStrategy::description() const
{
    return PkString("Keep only meta data that are identical");
}

void OnlyIdenticalMergeStrategy::merge(Store* dst, PkList<const Store*> srcs, PkList<double> score) const
{
    Q_UNUSED(score);
    dbgMetaData << "OnlyIdenticalMergeStrategy";
    dbgMetaData << "Priority to first meta data";

    Q_ASSERT(srcs.size() > 0);
    PkList<PkString> keys = srcs[0]->keys();
    for (const PkString & key : keys) {
        bool keep = true;
        const Entry& e = srcs[0]->getEntry(key);
        const Value& v = e.value();
        for (const Store* store : srcs) {
            if (!(store->containsEntry(key) && e.value() == v)) {
                keep = false;
                break;
            }
        }
        if (keep) {
            dst->addEntry(e);
        }
    }
}

//-------------------------------------------//
//------------ SmartMergeStrategy -----------//
//-------------------------------------------//

SmartMergeStrategy::SmartMergeStrategy()
{
}

SmartMergeStrategy::~SmartMergeStrategy()
{
}

PkString SmartMergeStrategy::id() const
{
    return "Smart";
}
PkString SmartMergeStrategy::name() const
{
    return PkString("Smart");
}

PkString SmartMergeStrategy::description() const
{
    return PkString("This merge strategy attempts to find the best solution for merging, "
                "for instance by merging the list of authors together, or keeping "
                "identical photographic information.");
}

struct ScoreValue {
    double score;
    Value value;
};

Value SmartMergeStrategy::election(PkList<const Store*> srcs, PkList<double> scores, const PkString & key) const
{
    PkList<ScoreValue> scoreValues;
    for (int i = 0; i < srcs.size(); i++) {
        if (srcs[i]->containsEntry(key)) {
            const Value& nv = srcs[i]->getEntry(key).value();
            if (nv.type() != Value::Invalid) {
                bool found = false;
                for (int j = 0; j < scoreValues.size(); j++) {
                    ScoreValue& sv = scoreValues[j];
                    if (sv.value == nv) {
                        found = true;
                        sv.score += scores[i];
                        break;
                    }
                }
                if (!found) {
                    ScoreValue sv;
                    sv.score = scores[i];
                    sv.value = nv;
                    scoreValues.append(sv);
                }
            }
        }
    }
    if (scoreValues.size() < 1) {
        warnMetaData << "SmartMergeStrategy::election returned less than 1 score value";
        return Value();
    }
    const ScoreValue* bestSv = 0;
    double bestScore = -1.0;
    for (const ScoreValue& sv : scoreValues) {
        if (sv.score > bestScore) {
            bestScore = sv.score;
            bestSv = &sv;
        }
    }
    if (bestSv) {
        return bestSv->value;
    }
    else {
        return Value();
    }
}

void SmartMergeStrategy::mergeEntry(Store* dst, PkList<const Store*> srcs, const KisMetaData::Schema* schema, const PkString & identifier) const
{
    bool foundOnce = false;
    Value v(PkList<Value>(), Value::OrderedArray);
    for (const Store* store : srcs) {
        if (store->containsEntry(schema, identifier)) {
            v += store->getEntry(schema, identifier).value();
            foundOnce = true;
        }
    }
    if (foundOnce) {
        dst->getEntry(schema, identifier).value() = v;
    }
}

void SmartMergeStrategy::merge(Store* dst, PkList<const Store*> srcs, PkList<double> scores) const
{
    dbgMetaData << "Smart merging of meta data";
    Q_ASSERT(srcs.size() == scores.size());
    Q_ASSERT(srcs.size() > 0);
    if (srcs.size() == 1) {
        dst->copyFrom(srcs[0]);
        return;
    }
    // Initialize some schema
    const KisMetaData::Schema* dcSchema = KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::DublinCoreSchemaUri);
//     const KisMetaData::Schema* psSchema = KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::PhotoshopSchemaUri);
    const KisMetaData::Schema* XMPRightsSchema = KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::XMPRightsSchemaUri);
    const KisMetaData::Schema* XMPSchema = KisMetaData::SchemaRegistry::instance()->schemaFromUri(KisMetaData::Schema::XMPSchemaUri);
    // Sort the stores and scores
    {
        // Qt 的多值映射无 pk 替代：std::multimap 按键升序迭代，
        // 升序语义与原实现（多值映射容器的 values()/keys()）等价。
        std::multimap<double, const Store*> scores2srcs;
        for (int i = 0; i < scores.size(); ++i) {
            scores2srcs.insert({scores[i], srcs[i]});
        }
        PkList<const Store*> sortedSrcs;
        PkList<double> sortedScores;
        for (const auto &entry : scores2srcs) {
            sortedSrcs.append(entry.second);
            sortedScores.append(entry.first);
        }
        srcs = sortedSrcs;
        scores = sortedScores;
    }

    // First attempt to see if one of the store has a higher score than the others
    if (scores[0] > 2 * scores[1]) { // One of the store has a higher importance than the other ones
        dst->copyFrom(srcs[0]);
    } else {
        // Merge exif info


        // Election
        for (const Store* store : srcs) {
            PkList<PkString> keys = store->keys();
            for (const PkString & key : keys) {
                if (!dst->containsEntry(key)) {
                    Value v = election(srcs, scores, key);
                    if (v.type() != Value::Invalid) {
                        dst->getEntry(key).value() = v;
                    }
                }
            }
        }

        // Compute rating
        double rating = 0.0;
        double norm = 0.0;
        for (int i = 0; i < srcs.size(); i++) {
            const Store* store = srcs[i];
            if (store->containsEntry(XMPSchema, "Rating")) {
                double score = scores[i];
                rating += score * store->getEntry(XMPSchema, "Rating").value().asVariant().toDouble();
                norm += score;
            }
        }
        if (norm > 0.01) {
            dst->getEntry(XMPSchema, "Rating").value() = PkVariant((int)(rating / norm));
        }
    }
    // Merge the list of authors and keywords and other stuff
    mergeEntry(dst, srcs, dcSchema, "contributor");
    mergeEntry(dst, srcs, dcSchema, "creator");
    mergeEntry(dst, srcs, dcSchema, "publisher");
    mergeEntry(dst, srcs, dcSchema, "subject");
    mergeEntry(dst, srcs, XMPRightsSchema, "Owner");
    mergeEntry(dst, srcs, XMPSchema, "Identifier");
}
