/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "transaction_manager.h"
#include "record/rm_file_handle.h"
#include "system/sm_manager.h"

std::unordered_map<txn_id_t, Transaction *> TransactionManager::txn_map = {};

/**
 * @description: 事务的开始方法
 */
Transaction * TransactionManager::begin(Transaction* txn, LogManager* log_manager) {
    // 1. 判断传入事务参数是否为空指针
    if (txn == nullptr) {
        // 2. 创建新事务
        txn_id_t txn_id = next_txn_id_.fetch_add(1);
        txn = new Transaction(txn_id);
        txn->set_state(TransactionState::GROWING);
    } else {
        // 已有事务（显式begin）
        txn->set_state(TransactionState::GROWING);
    }
    
    // 3. 把开始事务加入到全局事务表中
    std::unique_lock<std::mutex> lock(latch_);
    txn_map[txn->get_transaction_id()] = txn;
    lock.unlock();
    
    // 4. 记录begin日志
    if (log_manager != nullptr) {
        BeginLogRecord *begin_log = new BeginLogRecord(txn->get_transaction_id());
        lsn_t lsn = log_manager->add_log_to_buffer(begin_log);
        txn->set_prev_lsn(lsn);
        delete begin_log;
    }
    
    return txn;
}

/**
 * @description: 事务的提交方法
 */
void TransactionManager::commit(Transaction* txn, LogManager* log_manager) {
    if (txn == nullptr) return;
    
    // 1. 记录commit日志
    if (log_manager != nullptr) {
        CommitLogRecord *commit_log = new CommitLogRecord(txn->get_transaction_id());
        log_manager->add_log_to_buffer(commit_log);
        delete commit_log;
        // 4. 把事务日志刷入磁盘中
        log_manager->flush_log_to_disk();
    }
    
    // 2. 释放所有锁
    if (lock_manager_ != nullptr) {
        auto lock_set = txn->get_lock_set();
        for (auto &lock_id : *lock_set) {
            lock_manager_->unlock(txn, lock_id);
        }
    }
    
    // 3. 更新事务状态
    txn->set_state(TransactionState::COMMITTED);
    
    // 5. 从全局事务表中移除
    std::unique_lock<std::mutex> lock(latch_);
    txn_map.erase(txn->get_transaction_id());
    lock.unlock();
}

/**
 * @description: 事务的终止（回滚）方法
 */
void TransactionManager::abort(Transaction * txn, LogManager *log_manager) {
    if (txn == nullptr) return;
    
    // 1. 回滚所有写操作（逆序处理write_set）
    auto write_set = txn->get_write_set();
    while (!write_set->empty()) {
        WriteRecord *wr = write_set->back();
        write_set->pop_back();
        
        // 获取表文件句柄
        auto &fhs = sm_manager_->fhs_;
        if (fhs.find(wr->GetTableName()) != fhs.end()) {
            auto fh = fhs[wr->GetTableName()].get();
            switch (wr->GetWriteType()) {
                case WType::INSERT_TUPLE:
                    // 回滚插入：删除该记录
                    fh->delete_record(wr->GetRid(), nullptr);
                    break;
                case WType::DELETE_TUPLE:
                    // 回滚删除：恢复原记录
                    fh->insert_record(wr->GetRid(), wr->GetRecord().data);
                    break;
                case WType::UPDATE_TUPLE:
                    // 回滚更新：恢复旧值
                    fh->update_record(wr->GetRid(), wr->GetRecord().data, nullptr);
                    break;
            }
        }
        delete wr;
    }
    
    // 2. 释放所有锁
    if (lock_manager_ != nullptr) {
        auto lock_set = txn->get_lock_set();
        for (auto &lock_id : *lock_set) {
            lock_manager_->unlock(txn, lock_id);
        }
    }
    
    // 4. 记录abort日志
    if (log_manager != nullptr) {
        AbortLogRecord *abort_log = new AbortLogRecord(txn->get_transaction_id());
        log_manager->add_log_to_buffer(abort_log);
        delete abort_log;
        log_manager->flush_log_to_disk();
    }
    
    // 5. 更新事务状态
    txn->set_state(TransactionState::ABORTED);
    
    // 从全局事务表中移除
    std::unique_lock<std::mutex> lock(latch_);
    txn_map.erase(txn->get_transaction_id());
    lock.unlock();
}

// MVCC-related stub implementations (not needed for basic functionality)
bool TransactionManager::UpdateUndoLink(Rid rid, std::optional<UndoLink> prev_link,
                    std::function<bool(std::optional<UndoLink>)> &&check) { return false; }
bool TransactionManager::UpdateVersionLink(Rid rid, std::optional<VersionUndoLink> prev_version,
                   std::function<bool(std::optional<VersionUndoLink>)> &&check) { return false; }
std::optional<UndoLink> TransactionManager::GetUndoLink(Rid rid) { return std::nullopt; }
std::optional<VersionUndoLink> TransactionManager::GetVersionLink(Rid rid) { return std::nullopt; }
std::optional<UndoLog> TransactionManager::GetUndoLogOptional(UndoLink link) { return std::nullopt; }
UndoLog TransactionManager::GetUndoLog(UndoLink link) { return UndoLog(); }
timestamp_t TransactionManager::GetWatermark() { return 0; }
void TransactionManager::GarbageCollection() {}
