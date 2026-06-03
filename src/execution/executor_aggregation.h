/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE. */

#pragma once
#include <algorithm>
#include <unordered_map>
#include <sstream>
#include "execution_defs.h"
#include "execution_manager.h"
#include "executor_abstract.h"
#include "index/ix.h"
#include "system/sm.h"

// Aggregation function types
enum AggFuncType {
    AGG_NONE, AGG_COUNT_ALL, AGG_COUNT_COL, AGG_SUM, AGG_AVG, AGG_MIN, AGG_MAX
};

class AggregationExecutor : public AbstractExecutor {
   public:
    // Aggregation configuration
    struct AggColInfo {
        size_t src_idx;      // index in prev columns
        AggFuncType func;
        bool is_count_star;
    };
   private:
    std::vector<AggColInfo> agg_cols_;
    std::vector<std::string> group_by_cols_;

    // Results
    std::vector<std::unique_ptr<RmRecord>> results_;
    size_t result_idx_;

    // Helper: compare values
    static int val_compare(const char *a, const char *b, ColType type, int len) {
        if (type == TYPE_INT) {
            int ia = *(int *)a, ib = *(int *)b;
            return (ia < ib) ? -1 : ((ia > ib) ? 1 : 0);
        } else if (type == TYPE_FLOAT) {
            float fa = *(float *)a, fb = *(float *)b;
            return (fa < fb) ? -1 : ((fa > fb) ? 1 : 0);
        } else {
            return memcmp(a, b, len);
        }
    }

   public:
    AggregationExecutor(std::unique_ptr<AbstractExecutor> prev,
                        const std::vector<std::string> &group_by_cols,
                        const std::vector<AggColInfo> &agg_cols) {
        prev_ = std::move(prev);
        group_by_cols_ = group_by_cols;
        agg_cols_ = agg_cols;
        done_ = false;
        result_idx_ = 0;

        // Build output columns: group by columns + aggregation results
        auto &prev_cols = prev_->cols();
        size_t offset = 0;
        
        // Group-by columns
        for (auto &gb_col : group_by_cols_) {
            for (size_t i = 0; i < prev_cols.size(); i++) {
                if (prev_cols[i].name == gb_col) {
                    ColMeta col = prev_cols[i];
                    col.offset = offset;
                    offset += col.len;
                    cols_.push_back(col);
                    break;
                }
            }
        }
        
        // Aggregation result columns (one per agg func)
        for (auto &ac : agg_cols_) {
            ColMeta col;
            col.tab_name = "";
            col.len = ac.func == AGG_COUNT_ALL || ac.func == AGG_COUNT_COL ? sizeof(int) : sizeof(float);
            col.type = ac.func == AGG_COUNT_ALL || ac.func == AGG_COUNT_COL ? TYPE_INT : TYPE_FLOAT;
            switch (ac.func) {
                case AGG_COUNT_ALL: col.name = "count(*)"; break;
                case AGG_COUNT_COL: col.name = "count"; break;
                case AGG_SUM: col.name = "sum"; break;
                case AGG_AVG: col.name = "avg"; break;
                case AGG_MIN: col.name = "min"; break;
                case AGG_MAX: col.name = "max"; break;
                default: col.name = "agg"; break;
            }
            col.offset = offset;
            offset += col.len;
            cols_.push_back(col);
        }
        len_ = offset;
    }

    void beginTuple() override {
        if (done_) return;
        done_ = true;
        results_.clear();
        result_idx_ = 0;

        auto &prev_cols = prev_->cols();

        // Collect all tuples and aggregate
        prev_->beginTuple();

        if (group_by_cols_.empty()) {
            // Simple aggregation (no GROUP BY)
            int count = 0;
            std::vector<float> sums(agg_cols_.size(), 0.0f);
            std::vector<float> mins(agg_cols_.size(), 1e30f);
            std::vector<float> maxs(agg_cols_.size(), -1e30f);
            std::vector<int> count_nulls(agg_cols_.size(), 0);

            while (!prev_->is_end()) {
                auto rec = prev_->Next();
                count++;
                for (size_t i = 0; i < agg_cols_.size(); i++) {
                    auto &ac = agg_cols_[i];
                    if (ac.func == AGG_COUNT_ALL) continue;
                    if (ac.is_count_star) continue;
                    
                    const char *data = rec->data + prev_cols[ac.src_idx].offset;
                    float val = 0;
                    if (prev_cols[ac.src_idx].type == TYPE_INT) {
                        val = (float)(*(int *)data);
                    } else if (prev_cols[ac.src_idx].type == TYPE_FLOAT) {
                        val = *(float *)data;
                    }
                    sums[i] += val;
                    if (val < mins[i]) mins[i] = val;
                    if (val > maxs[i]) maxs[i] = val;
                    count_nulls[i]++;
                }
                prev_->nextTuple();
            }

            auto result = std::make_unique<RmRecord>(len_);
            size_t offset = 0;
            for (auto &col : cols_) {
                if (col.type == TYPE_INT) {
                    *(int *)(result->data + col.offset) = 0;
                } else {
                    *(float *)(result->data + col.offset) = 0.0f;
                }
            }
            for (size_t i = 0; i < agg_cols_.size(); i++) {
                auto &ac = agg_cols_[i];
                auto &col = cols_[group_by_cols_.size() + i];
                switch (ac.func) {
                    case AGG_COUNT_ALL: *(int *)(result->data + col.offset) = count; break;
                    case AGG_COUNT_COL: *(int *)(result->data + col.offset) = count_nulls[i]; break;
                    case AGG_SUM: *(float *)(result->data + col.offset) = sums[i]; break;
                    case AGG_AVG: *(float *)(result->data + col.offset) = count > 0 ? sums[i] / count_nulls[i] : 0; break;
                    case AGG_MIN: *(float *)(result->data + col.offset) = mins[i]; break;
                    case AGG_MAX: *(float *)(result->data + col.offset) = maxs[i]; break;
                }
            }
            results_.push_back(std::move(result));
        } else {
            // GROUP BY aggregation
            std::map<std::string, std::vector<std::vector<float>>> groups;  // key -> [agg_values]

            while (!prev_->is_end()) {
                auto rec = prev_->Next();
                
                // Build group key
                std::string group_key;
                for (auto &gb_col : group_by_cols_) {
                    for (size_t j = 0; j < prev_cols.size(); j++) {
                        if (prev_cols[j].name == gb_col) {
                            group_key.append(rec->data + prev_cols[j].offset, prev_cols[j].len);
                            break;
                        }
                    }
                }

                if (groups.find(group_key) == groups.end()) {
                    groups[group_key] = std::vector<std::vector<float>>(agg_cols_.size());
                }
                auto &agg_vals = groups[group_key];

                for (size_t i = 0; i < agg_cols_.size(); i++) {
                    auto &ac = agg_cols_[i];
                    if (ac.func == AGG_COUNT_ALL) continue;
                    const char *data = rec->data + prev_cols[ac.src_idx].offset;
                    float val = 0;
                    if (prev_cols[ac.src_idx].type == TYPE_INT) {
                        val = (float)(*(int *)data);
                    } else if (prev_cols[ac.src_idx].type == TYPE_FLOAT) {
                        val = *(float *)data;
                    }
                    agg_vals[i].push_back(val);
                }
                prev_->nextTuple();
            }

            for (auto &[group_key, agg_vals] : groups) {
                auto result = std::make_unique<RmRecord>(len_);
                // Copy group-by key into result
                size_t gb_offset = 0;
                size_t key_pos = 0;
                for (auto &gb_col : group_by_cols_) {
                    for (size_t j = 0; j < prev_cols.size(); j++) {
                        if (prev_cols[j].name == gb_col && j < prev_cols.size()) {
                            memcpy(result->data + cols_[gb_offset].offset,
                                   group_key.data() + key_pos,
                                   std::min((size_t)prev_cols[j].len, group_key.length() - key_pos));
                            key_pos += prev_cols[j].len;
                            gb_offset++;
                            break;
                        }
                    }
                }

                for (size_t i = 0; i < agg_cols_.size(); i++) {
                    auto &ac = agg_cols_[i];
                    auto &col = cols_[group_by_cols_.size() + i];
                    auto &vals = agg_vals[i];
                    int count = vals.size();
                    switch (ac.func) {
                        case AGG_COUNT_ALL: *(int *)(result->data + col.offset) = count; break;
                        case AGG_COUNT_COL: *(int *)(result->data + col.offset) = count; break;
                        case AGG_SUM: {
                            float sum = 0; for (auto v : vals) sum += v;
                            *(float *)(result->data + col.offset) = sum; break;
                        }
                        case AGG_AVG: {
                            float sum = 0; for (auto v : vals) sum += v;
                            *(float *)(result->data + col.offset) = count > 0 ? sum / count : 0; break;
                        }
                        case AGG_MIN: {
                            float m = 1e30f; for (auto v : vals) m = std::min(m, v);
                            *(float *)(result->data + col.offset) = m; break;
                        }
                        case AGG_MAX: {
                            float m = -1e30f; for (auto v : vals) m = std::max(m, v);
                            *(float *)(result->data + col.offset) = m; break;
                        }
                    }
                }
                results_.push_back(std::move(result));
            }
        }
    }

    void nextTuple() override { result_idx_++; }

    bool is_end() const override { return result_idx_ >= results_.size(); }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end()) return nullptr;
        return std::make_unique<RmRecord>(*results_[result_idx_]);
    }

    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta> &cols() const override { return cols_; }
    Rid &rid() override { return _abstract_rid; }
};
