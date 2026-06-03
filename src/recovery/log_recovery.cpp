/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "log_recovery.h"
#include <unordered_set>
#include <algorithm>
#include <fstream>

// Helper: get checkpoint offset from restart file
static int get_checkpoint_offset() {
    std::ifstream restart_file("restart.txt");
    if (restart_file.is_open()) {
        int offset = 0;
        restart_file >> offset;
        restart_file.close();
        return offset;
    }
    return 0;  // No checkpoint, start from beginning
}

/**
 * @description: analyze阶段 - 从静态检查点开始扫描，识别活跃/已提交事务
 */
void RecoveryManager::analyze() {
    active_txns_.clear();
    committed_txns_.clear();
    
    // 从重启文件中读取检查点位置，实现 <70% 恢复时间
    int log_offset = get_checkpoint_offset();
    
    char log_buf[LOG_BUFFER_SIZE];
    int bytes_read = disk_manager_->read_log(log_buf, LOG_BUFFER_SIZE, log_offset);
    
    while (bytes_read > 0) {
        int offset = 0;
        while (offset < bytes_read) {
            LogType log_type = *reinterpret_cast<LogType*>(log_buf + offset);
            int log_tot_len = *reinterpret_cast<uint32_t*>(log_buf + offset + OFFSET_LOG_TOT_LEN);
            txn_id_t log_tid = *reinterpret_cast<txn_id_t*>(log_buf + offset + OFFSET_LOG_TID);
            
            if (log_tot_len <= 0) break;
            
            switch (log_type) {
                case LogType::begin:
                    active_txns_.insert(log_tid);
                    break;
                case LogType::commit:
                    active_txns_.erase(log_tid);
                    committed_txns_.insert(log_tid);
                    break;
                case LogType::ABORT:
                    active_txns_.erase(log_tid);
                    break;
                default:
                    break;
            }
            
            offset += log_tot_len;
        }
        
        log_offset += bytes_read;
        if (log_offset >= disk_manager_->get_file_size(LOG_FILE_NAME)) break;
        bytes_read = disk_manager_->read_log(log_buf, LOG_BUFFER_SIZE, log_offset);
    }
}

/**
 * @description: redo阶段 - 必须重演历史 (Repeating History)
 * ARIES铁律：不管事务最终 Commit 还是 Abort，只要是落盘的日志，全部无脑 Redo！
 * 原因：RMDB使用STEAL策略，未提交事务的脏页可能已被刷盘。
 * 如果不重做所有操作，undo阶段的补偿操作会在错误的数据状态上执行，导致崩溃。
 */
void RecoveryManager::redo() {
    int log_offset = get_checkpoint_offset();
    char log_buf[LOG_BUFFER_SIZE];
    int bytes_read = disk_manager_->read_log(log_buf, LOG_BUFFER_SIZE, log_offset);
    
    while (bytes_read > 0) {
        int offset = 0;
        while (offset < bytes_read) {
            LogType log_type = *reinterpret_cast<LogType*>(log_buf + offset);
            int log_tot_len = *reinterpret_cast<uint32_t*>(log_buf + offset + OFFSET_LOG_TOT_LEN);
            
            if (log_tot_len <= 0) break;
            
            // 全部 Redo，不区分是否已提交！这是 Repeating History 原则
            switch (log_type) {
                case LogType::INSERT: {
                    InsertLogRecord rec;
                    rec.deserialize(log_buf + offset);
                    if (sm_manager_->fhs_.count(std::string(rec.table_name_))) {
                        auto fh = sm_manager_->fhs_[std::string(rec.table_name_)].get();
                        fh->insert_record(rec.rid_, rec.insert_value_.data);
                    }
                    break;
                }
                case LogType::DELETE: {
                    DeleteLogRecord rec;
                    rec.deserialize(log_buf + offset);
                    if (sm_manager_->fhs_.count(std::string(rec.table_name_))) {
                        auto fh = sm_manager_->fhs_[std::string(rec.table_name_)].get();
                        fh->delete_record(rec.rid_, nullptr);
                    }
                    break;
                }
                case LogType::UPDATE: {
                    UpdateLogRecord rec;
                    rec.deserialize(log_buf + offset);
                    if (sm_manager_->fhs_.count(std::string(rec.table_name_))) {
                        auto fh = sm_manager_->fhs_[std::string(rec.table_name_)].get();
                        fh->update_record(rec.rid_, rec.new_value_.data, nullptr);
                    }
                    break;
                }
                default:
                    break;
            }
            
            offset += log_tot_len;
        }
        
        log_offset += bytes_read;
        if (log_offset >= disk_manager_->get_file_size(LOG_FILE_NAME)) break;
        bytes_read = disk_manager_->read_log(log_buf, LOG_BUFFER_SIZE, log_offset);
    }
}

/**
 * @description: undo阶段 - 回滚未完成的事务
 */
void RecoveryManager::undo() {
    if (active_txns_.empty()) return;
    
    // 正序扫描收集活跃事务的操作日志
    int log_pos = 0;
    char scan_buf[LOG_BUFFER_SIZE];
    int scan_bytes = disk_manager_->read_log(scan_buf, LOG_BUFFER_SIZE, log_pos);
    
    std::unordered_map<txn_id_t, std::vector<std::pair<char*, int>>> txn_ops;
    
    while (scan_bytes > 0) {
        int off = 0;
        while (off < scan_bytes) {
            LogType type = *reinterpret_cast<LogType*>(scan_buf + off);
            int tot_len = *reinterpret_cast<uint32_t*>(scan_buf + off + OFFSET_LOG_TOT_LEN);
            txn_id_t tid = *reinterpret_cast<txn_id_t*>(scan_buf + off + OFFSET_LOG_TID);
            
            if (tot_len <= 0) break;
            
            if (active_txns_.count(tid)) {
                txn_ops[tid].push_back({scan_buf + off, off});
            }
            
            off += tot_len;
        }
        
        log_pos += scan_bytes;
        if (log_pos >= disk_manager_->get_file_size(LOG_FILE_NAME)) break;
        scan_bytes = disk_manager_->read_log(scan_buf, LOG_BUFFER_SIZE, log_pos);
    }
    
    // 对每个活跃事务，反向处理其操作（逆序undo）
    for (auto &[tid, ops] : txn_ops) {
        std::reverse(ops.begin(), ops.end());
        for (auto &[buf_ptr, _] : ops) {
            LogType type = *reinterpret_cast<LogType*>(buf_ptr);
            switch (type) {
                case LogType::INSERT: {
                    InsertLogRecord rec;
                    rec.deserialize(buf_ptr);
                    if (sm_manager_->fhs_.count(std::string(rec.table_name_))) {
                        auto fh = sm_manager_->fhs_[std::string(rec.table_name_)].get();
                        fh->delete_record(rec.rid_, nullptr);
                    }
                    break;
                }
                case LogType::DELETE: {
                    DeleteLogRecord rec;
                    rec.deserialize(buf_ptr);
                    if (sm_manager_->fhs_.count(std::string(rec.table_name_))) {
                        auto fh = sm_manager_->fhs_[std::string(rec.table_name_)].get();
                        fh->insert_record(rec.rid_, rec.delete_value_.data);
                    }
                    break;
                }
                case LogType::UPDATE: {
                    UpdateLogRecord rec;
                    rec.deserialize(buf_ptr);
                    if (sm_manager_->fhs_.count(std::string(rec.table_name_))) {
                        auto fh = sm_manager_->fhs_[std::string(rec.table_name_)].get();
                        fh->update_record(rec.rid_, rec.old_value_.data, nullptr);
                    }
                    break;
                }
                default:
                    break;
            }
        }
    }
}
