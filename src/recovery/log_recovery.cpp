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

/**
 * @description: analyze阶段 - 扫描日志识别活跃事务和脏页
 * 从最新的静态检查点开始扫描，构建 undo_list (未完成事务) 和 redo_list (需要重做的事务)
 */
void RecoveryManager::analyze() {
    // 简化实现：扫描所有日志，识别 BEGIN 和 COMMIT/ABORT
    // 处于活跃状态的事务（有 BEGIN 但无 COMMIT/ABORT）需要 undo
    active_txns_.clear();
    committed_txns_.clear();
    
    // Read log file from beginning (or from last checkpoint)
    int log_offset = 0;
    char log_buf[LOG_BUFFER_SIZE];
    int bytes_read = disk_manager_->read_log(log_buf, LOG_BUFFER_SIZE, log_offset);
    
    while (bytes_read > 0) {
        int offset = 0;
        while (offset < bytes_read) {
            LogType log_type = *reinterpret_cast<LogType*>(log_buf + offset);
            int log_tot_len = *reinterpret_cast<uint32_t*>(log_buf + offset + OFFSET_LOG_TOT_LEN);
            txn_id_t log_tid = *reinterpret_cast<txn_id_t*>(log_buf + offset + OFFSET_LOG_TID);
            
            if (log_tot_len <= 0) break;  // corrupted or end
            
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
 * @description: redo阶段 - 重做已提交事务的所有操作
 * 从日志起始位置扫描，重做所有 INSERT/DELETE/UPDATE 操作
 */
void RecoveryManager::redo() {
    int log_offset = 0;
    char log_buf[LOG_BUFFER_SIZE];
    int bytes_read = disk_manager_->read_log(log_buf, LOG_BUFFER_SIZE, log_offset);
    
    while (bytes_read > 0) {
        int offset = 0;
        while (offset < bytes_read) {
            LogType log_type = *reinterpret_cast<LogType*>(log_buf + offset);
            int log_tot_len = *reinterpret_cast<uint32_t*>(log_buf + offset + OFFSET_LOG_TOT_LEN);
            txn_id_t log_tid = *reinterpret_cast<txn_id_t*>(log_buf + offset + OFFSET_LOG_TID);
            
            if (log_tot_len <= 0) break;
            
            // 只重做已提交事务的操作
            if (committed_txns_.count(log_tid)) {
                switch (log_type) {
                    case LogType::INSERT: {
                        InsertLogRecord rec;
                        rec.deserialize(log_buf + offset);
                        // Re-insert the record at the logged position
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
 * 对 active_txns_ 中的每个事务，反向遍历其日志并执行补偿操作
 */
void RecoveryManager::undo() {
    if (active_txns_.empty()) return;
    
    // 反向扫描日志，对活跃事务执行补偿操作
    int file_size = disk_manager_->get_file_size(LOG_FILE_NAME);
    int log_offset = std::max(0, file_size - LOG_BUFFER_SIZE);
    char log_buf[LOG_BUFFER_SIZE];
    
    while (log_offset >= 0) {
        int bytes_read = disk_manager_->read_log(log_buf, LOG_BUFFER_SIZE, log_offset);
        if (bytes_read <= 0) { log_offset -= LOG_BUFFER_SIZE; continue; }
        
        int offset = bytes_read;
        while (offset > 0) {
            // 从后向前查找日志边界（简化：从每条记录的 tot_len 定位）
            // 实际上需要从文件起始正序读取并记录每条日志的位置和类型
            // 这里采用简化实现：正序读取一次，记录所有需要 undo 的日志位置
            offset--;
        }
        
        log_offset -= LOG_BUFFER_SIZE;
    }
    
    // 简化实现：正序扫描，为活跃事务收集undo信息
    // 实际上应对每个活跃事务，从后往前遍历其日志链并执行补偿
    // 由于UndoLog链由prev_lsn_连接，这里执行基本回滚
    
    int log_pos = 0;
    char scan_buf[LOG_BUFFER_SIZE];
    int scan_bytes = disk_manager_->read_log(scan_buf, LOG_BUFFER_SIZE, log_pos);
    
    // 为每个活跃事务收集操作（按LSN排序，后续反向处理）
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
