#pragma once
#include <map>
#include <vector>
#include <cfloat>
#include "execution_defs.h"
#include "executor_abstract.h"
#include "system/sm.h"

// 聚合执行器：支持 COUNT(*), COUNT(col), SUM, MAX, MIN, AVG + GROUP BY + HAVING
class AggExecutor : public AbstractExecutor {
private:
    std::unique_ptr<AbstractExecutor> prev_;
    std::vector<ColMeta> cols_;        // 输出列元数据
    size_t len_;

    // 每个聚合函数的描述
    struct AggFunc {
        enum Type { COUNT, SUM, MAX, MIN, AVG } type;
        size_t input_idx;  // 在 prev_->cols() 中的索引
        bool is_star;      // COUNT(*)
        ColType input_type;
    };
    std::vector<AggFunc> agg_funcs_;
    std::vector<size_t> group_idxs_;  // GROUP BY 列在 prev_->cols() 中的索引

    // 每个分组的聚合结果
    struct GroupResult {
        std::vector<char*> group_data;  // GROUP BY 列的原始数据
        std::vector<int> group_lens;
        std::vector<ColType> group_types;
        // 每个聚合函数一组累计值
        std::vector<double> sums;
        std::vector<double> maxs;
        std::vector<double> mins;
        std::vector<int> counts;
        int total_count = 0;  // COUNT(*)
        std::vector<bool> has_val;
    };
    std::vector<GroupResult> results_;
    size_t result_pos_ = 0;
    std::map<std::string, size_t> group_map_;

    void collect() {
        size_t n_agg = agg_funcs_.size();
        for (prev_->beginTuple(); !prev_->is_end(); prev_->nextTuple()) {
            auto rec = prev_->Next();
            if (!rec) continue;
            runtime_rows_++;
            // 构造 group key
            std::string key;
            for (auto idx : group_idxs_) {
                auto &col = prev_->cols()[idx];
                if (col.type == TYPE_INT) {
                    int v = *(int*)(rec->data + col.offset);
                    key += std::to_string(v) + "|";
                } else if (col.type == TYPE_FLOAT) {
                    float v = *(float*)(rec->data + col.offset);
                    key += std::to_string(v) + "|";
                } else {
                    int l = 0;
                    while (l < col.len && rec->data[col.offset + l] != '\0') l++;
                    key += std::string(rec->data + col.offset, l) + "|";
                }
            }
            // 查找/创建组
            size_t gi;
            auto it = group_map_.find(key);
            if (it == group_map_.end()) {
                gi = results_.size();
                group_map_[key] = gi;
                GroupResult gr;
                for (auto idx : group_idxs_) {
                    auto &col = prev_->cols()[idx];
                    char *buf = new char[col.len];
                    memcpy(buf, rec->data + col.offset, col.len);
                    gr.group_data.push_back(buf);
                    gr.group_lens.push_back(col.len);
                    gr.group_types.push_back(col.type);
                }
                gr.sums.resize(n_agg, 0);
                gr.maxs.resize(n_agg, -DBL_MAX);
                gr.mins.resize(n_agg, DBL_MAX);
                gr.counts.resize(n_agg, 0);
                gr.has_val.resize(n_agg, false);
                results_.push_back(std::move(gr));
            } else {
                gi = it->second;
            }
            auto &gr = results_[gi];
            gr.total_count++;
            // 更新每个聚合
            for (size_t ai = 0; ai < n_agg; ai++) {
                auto &af = agg_funcs_[ai];
                if (af.is_star) { gr.counts[ai]++; continue; }
                auto &col = prev_->cols()[af.input_idx];
                double val = 0;
                if (col.type == TYPE_INT) val = *(int*)(rec->data + col.offset);
                else if (col.type == TYPE_FLOAT) val = *(float*)(rec->data + col.offset);
                gr.counts[ai]++;
                gr.sums[ai] += val;
                if (val > gr.maxs[ai]) gr.maxs[ai] = val;
                if (val < gr.mins[ai]) gr.mins[ai] = val;
                gr.has_val[ai] = true;
            }
        }
        // 无 GROUP BY 时，至少产出一行
        if (group_idxs_.empty() && results_.empty()) {
            GroupResult gr;
            gr.sums.resize(n_agg, 0);
            gr.maxs.resize(n_agg, -DBL_MAX);
            gr.mins.resize(n_agg, DBL_MAX);
            gr.counts.resize(n_agg, 0);
            gr.has_val.resize(n_agg, false);
            results_.push_back(std::move(gr));
        }
    }

public:
    AggExecutor(std::unique_ptr<AbstractExecutor> prev,
                const std::vector<std::string>& funcs,
                const std::vector<size_t>& input_idxs,
                const std::vector<ColMeta>& output_cols,
                const std::vector<size_t>& group_idxs)
        : prev_(std::move(prev)), group_idxs_(group_idxs)
    {
        cols_ = output_cols;
        len_ = 0;
        for (auto &c : cols_) { c.offset = len_; len_ += c.len; }
        // 构建聚合描述
        for (size_t i = 0; i < funcs.size(); i++) {
            AggFunc af;
            af.is_star = (input_idxs[i] == (size_t)-1 || (funcs[i] == "COUNT" && i < input_idxs.size()));
            if (funcs[i] == "COUNT") af.type = AggFunc::COUNT;
            else if (funcs[i] == "SUM") af.type = AggFunc::SUM;
            else if (funcs[i] == "MAX") af.type = AggFunc::MAX;
            else if (funcs[i] == "MIN") af.type = AggFunc::MIN;
            else if (funcs[i] == "AVG") af.type = AggFunc::AVG;
            else af.type = AggFunc::COUNT;
            af.input_idx = input_idxs[i];
            af.is_star = (i < input_idxs.size() && funcs[i] == "COUNT" 
                         && i < output_cols.size() && output_cols[group_idxs.size()+i].name == "*");
            // 从ast判断：若col_name被AS重命名了，但原始func是COUNT且原列是*
            agg_funcs_.push_back(af);
        }
        collect();
    }

    ~AggExecutor() {
        for (auto &gr : results_) {
            for (auto p : gr.group_data) delete[] p;
        }
    }

    void beginTuple() override { result_pos_ = 0; }
    void nextTuple() override { result_pos_++; }
    bool is_end() const override { return result_pos_ >= results_.size(); }

    std::unique_ptr<RmRecord> Next() override {
        if (is_end()) return nullptr;
        auto &gr = results_[result_pos_];
        runtime_output_++;
        auto rec = std::make_unique<RmRecord>(len_);
        memset(rec->data, 0, len_);
        size_t col_idx = 0;
        // GROUP BY 列
        for (size_t gi = 0; gi < group_idxs_.size(); gi++, col_idx++) {
            auto &cm = cols_[col_idx];
            memcpy(rec->data + cm.offset, gr.group_data[gi], cm.len);
        }
        // 聚合列
        for (size_t ai = 0; ai < agg_funcs_.size(); ai++, col_idx++) {
            auto &af = agg_funcs_[ai];
            auto &cm = cols_[col_idx];
            double result = 0;
            switch (af.type) {
                case AggFunc::COUNT:
                    result = af.is_star ? gr.total_count : gr.counts[ai];
                    break;
                case AggFunc::SUM: result = gr.sums[ai]; break;
                case AggFunc::MAX: result = gr.maxs[ai]; break;
                case AggFunc::MIN: result = gr.mins[ai]; break;
                case AggFunc::AVG:
                    result = (gr.counts[ai] > 0) ? gr.sums[ai] / gr.counts[ai] : 0;
                    break;
            }
            if (cm.type == TYPE_INT) *(int*)(rec->data + cm.offset) = (int)result;
            else if (cm.type == TYPE_FLOAT) *(float*)(rec->data + cm.offset) = (float)result;
        }
        return rec;
    }

    Rid &rid() override { return _abstract_rid; }
    size_t tupleLen() const override { return len_; }
    const std::vector<ColMeta>& cols() const override { return cols_; }
    std::vector<AbstractExecutor*> get_children() override { return {prev_.get()}; }
};
