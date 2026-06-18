#pragma once
#include <map>
#include <vector>
#include "execution_defs.h"
#include "executor_abstract.h"
#include "system/sm.h"

// 聚合执行器：支持 COUNT(*), COUNT(col), SUM, MAX, MIN, AVG + GROUP BY
class AggExecutor : public AbstractExecutor {
private:
    std::unique_ptr<AbstractExecutor> prev_;
    std::vector<ColMeta> cols_;        // 输出列元数据
    size_t len_;

    // 聚合列描述
    struct AggCol {
        size_t input_idx;     // 在 prev_->cols() 中的索引
        ColType type;
        int len;
        bool is_star;         // COUNT(*)
        enum AggFunc { COUNT, SUM, MAX, MIN, AVG } func;
    };
    std::vector<AggCol> agg_cols_;
    std::vector<size_t> group_idxs_;   // GROUP BY 列在 prev_->cols() 中的索引

    // 结果缓存
    struct AggResult {
        std::vector<std::string> group_vals;
        int count_val = 0;
        double sum_val = 0.0;
        double max_val = 0.0;
        double min_val = 0.0;
        int count_star = 0;  // COUNT(*) 专用
        bool has_max = false;
        bool has_min = false;
    };
    std::vector<AggResult> results_;
    size_t result_pos_ = 0;
    std::map<std::string, size_t> group_map_;  // group_key -> results_ index

    void collect_aggregates() {
        // 遍历所有输入记录
        for (prev_->beginTuple(); !prev_->is_end(); prev_->nextTuple()) {
            auto rec = prev_->Next();
            if (rec == nullptr) continue;
            runtime_rows_++;

            // 构造 group key
            std::string group_key;
            for (auto idx : group_idxs_) {
                auto &col = prev_->cols()[idx];
                char buf[256] = {};
                if (col.type == TYPE_INT) {
                    snprintf(buf, sizeof(buf), "%d:", *(int*)(rec->data + col.offset));
                } else if (col.type == TYPE_FLOAT) {
                    snprintf(buf, sizeof(buf), "%f:", *(float*)(rec->data + col.offset));
                } else {
                    int l = 0;
                    while (l < col.len && rec->data[col.offset + l] != '\0') l++;
                    snprintf(buf, sizeof(buf), "%.*s:", l, rec->data + col.offset);
                }
                group_key += buf;
            }

            // 查找或创建组
            size_t grp_idx;
            auto it = group_map_.find(group_key);
            if (it == group_map_.end()) {
                grp_idx = results_.size();
                group_map_[group_key] = grp_idx;
                AggResult ar;
                // 保存 GROUP BY 值
                for (auto idx : group_idxs_) {
                    auto &col = prev_->cols()[idx];
                    std::string val;
                    if (col.type == TYPE_INT)
                        val = std::to_string(*(int*)(rec->data + col.offset));
                    else if (col.type == TYPE_FLOAT) {
                        char buf[32]; snprintf(buf, sizeof(buf), "%.6f", *(float*)(rec->data + col.offset));
                        val = buf;
                    } else {
                        int l = 0;
                        while (l < col.len && rec->data[col.offset + l] != '\0') l++;
                        val = std::string(rec->data + col.offset, l);
                    }
                    ar.group_vals.push_back(val);
                }
                results_.push_back(std::move(ar));
            } else {
                grp_idx = it->second;
            }

            // 聚合计算
            auto &ar = results_[grp_idx];
            for (auto &ac : agg_cols_) {
                double val = 0;
                if (!ac.is_star) {
                    auto &col = prev_->cols()[ac.input_idx];
                    if (col.type == TYPE_INT)
                        val = (double)*(int*)(rec->data + col.offset);
                    else if (col.type == TYPE_FLOAT)
                        val = (double)*(float*)(rec->data + col.offset);
                }
                switch (ac.func) {
                    case AggCol::COUNT:
                        ar.count_val++;
                        break;
                    case AggCol::SUM:
                        ar.sum_val += val;
                        break;
                    case AggCol::MAX:
                        if (!ar.has_max || val > ar.max_val) { ar.max_val = val; ar.has_max = true; }
                        break;
                    case AggCol::MIN:
                        if (!ar.has_min || val < ar.min_val) { ar.min_val = val; ar.has_min = true; }
                        break;
                    case AggCol::AVG:
                        ar.sum_val += val;
                        ar.count_val++;
                        break;
                }
            }
            ar.count_star++;  // COUNT(*): 每个输入记录计数一次
        }
    }

public:
    AggExecutor(std::unique_ptr<AbstractExecutor> prev,
                const std::vector<std::string>& agg_funcs,
                const std::vector<size_t>& agg_input_idxs,
                const std::vector<ColMeta>& output_cols,
                const std::vector<size_t>& group_idxs)
        : prev_(std::move(prev)), group_idxs_(group_idxs)
    {
        cols_ = output_cols;
        len_ = 0;
        for (auto &c : cols_) {
            c.offset = len_;
            len_ += c.len;
        }
        // 构建聚合列描述（基于 prev_->cols()）
        for (size_t i = 0; i < agg_funcs.size(); i++) {
            AggCol ac;
            ac.func = (agg_funcs[i] == "COUNT") ? AggCol::COUNT :
                      (agg_funcs[i] == "SUM") ? AggCol::SUM :
                      (agg_funcs[i] == "MAX") ? AggCol::MAX :
                      (agg_funcs[i] == "MIN") ? AggCol::MIN :
                      (agg_funcs[i] == "AVG") ? AggCol::AVG : AggCol::COUNT;
            ac.is_star = (agg_funcs[i] == "STAR");  // COUNT(*)
            ac.input_idx = (ac.is_star) ? 0 : agg_input_idxs[i];
            if (!ac.is_star) {
                auto &c = prev_->cols()[ac.input_idx];
                ac.type = c.type;
                ac.len = c.len;
            }
            agg_cols_.push_back(ac);
        }
        collect_aggregates();
    }

    void beginTuple() override { result_pos_ = 0; }
    void nextTuple() override { result_pos_++; }
    bool is_end() const override { return result_pos_ >= results_.size(); }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end()) return nullptr;
        auto &ar = results_[result_pos_];
        runtime_output_++;
        auto rec = std::make_unique<RmRecord>(len_);
        size_t col_idx = 0;

        for (size_t gi = 0; gi < group_idxs_.size(); gi++, col_idx++) {
            auto &col = cols_[col_idx];
            std::string &val = ar.group_vals[gi];
            if (col.type == TYPE_INT)
                *(int*)(rec->data + col.offset) = std::stoi(val);
            else if (col.type == TYPE_FLOAT)
                *(float*)(rec->data + col.offset) = std::stof(val);
            else if (col.type == TYPE_STRING) {
                memset(rec->data + col.offset, 0, col.len);
                memcpy(rec->data + col.offset, val.c_str(), std::min(val.size(), (size_t)col.len));
            }
        }
        for (size_t ai = 0; ai < agg_cols_.size(); ai++, col_idx++) {
            auto &ac = agg_cols_[ai];
            auto &col = cols_[col_idx];
            double result = 0;
            switch (ac.func) {
                case AggCol::COUNT: result = ar.count_val; break;
                case AggCol::SUM:   result = ar.sum_val; break;
                case AggCol::MAX:   result = ar.max_val; break;
                case AggCol::MIN:   result = ar.min_val; break;
                case AggCol::AVG:   result = (ar.count_val > 0) ? ar.sum_val / ar.count_val : 0; break;
            }
            if (col.type == TYPE_INT)
                *(int*)(rec->data + col.offset) = (int)result;
            else if (col.type == TYPE_FLOAT)
                *(float*)(rec->data + col.offset) = (float)result;
            else if (col.type == TYPE_STRING && ac.func == AggCol::COUNT && ac.is_star) {
                // COUNT(*) → INT column
                *(int*)(rec->data + col.offset) = ar.count_star;
            }
        }
        return rec;
    }

    Rid &rid() override { return _abstract_rid; }
    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta>& cols() const override { return cols_; }
    std::vector<AbstractExecutor*> get_children() override { return {prev_.get()}; }
};
