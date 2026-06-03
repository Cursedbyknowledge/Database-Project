/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include <cstring>
#include "log_manager.h"
#include "storage/disk_manager.h"

/**
 * @description: 添加日志记录到日志缓冲区中，并返回日志记录号
 */
lsn_t LogManager::add_log_to_buffer(LogRecord* log_record) {
    std::unique_lock<std::mutex> lock(latch_);
    
    // 分配LSN
    lsn_t lsn = global_lsn_.fetch_add(1);
    log_record->lsn_ = lsn;
    
    // 序列化日志记录
    char buf[log_record->log_tot_len_];
    log_record->serialize(buf);
    
    // 如果缓冲区满，先刷盘
    if (log_buffer_.is_full(log_record->log_tot_len_)) {
        lock.unlock();
        flush_log_to_disk();
        lock.lock();
    }
    
    // 写入缓冲区
    memcpy(log_buffer_.buffer_ + log_buffer_.offset_, buf, log_record->log_tot_len_);
    log_buffer_.offset_ += log_record->log_tot_len_;
    
    return lsn;
}

/**
 * @description: 把日志缓冲区的内容刷到磁盘中
 */
void LogManager::flush_log_to_disk() {
    if (log_buffer_.offset_ == 0) return;
    
    // 使用DiskManager的日志写入方法
    disk_manager_->write_log(log_buffer_.buffer_, log_buffer_.offset_);
    
    persist_lsn_ = global_lsn_.load() - 1;
    
    // 清空缓冲区
    memset(log_buffer_.buffer_, 0, LOG_BUFFER_SIZE);
    log_buffer_.offset_ = 0;
}
