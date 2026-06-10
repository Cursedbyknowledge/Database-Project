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

Transaction * TransactionManager::begin(Transaction* txn, LogManager* log_manager) {
    // 1. 判断传入事务参数是否为空指针
    if (txn == nullptr) {
        // 2. 如果为空指针，创建新事务
        txn_id_t txn_id = next_txn_id_.fetch_add(1);
        txn = new Transaction(txn_id);
        timestamp_t start_ts = next_timestamp_.fetch_add(1);
        txn->set_start_ts(start_ts);
    }
    // 3. 把开始事务加入到全局事务表中
    std::unique_lock<std::mutex> lock(latch_);
    txn_map[txn->get_transaction_id()] = txn;
    lock.unlock();

    // 写 BEGIN 日志
    if (log_manager != nullptr) {
        auto log_rec = new BeginLogRecord(txn->get_transaction_id());
        txn->set_prev_lsn(log_manager->add_log_to_buffer(log_rec));
    }

    txn->set_state(TransactionState::GROWING);

    // 4. 返回当前事务指针
    return txn;
}

void TransactionManager::commit(Transaction* txn, LogManager* log_manager) {
    // 写 COMMIT 日志
    if (log_manager != nullptr) {
        auto log_rec = new CommitLogRecord();
        log_rec->log_tid_ = txn->get_transaction_id();
        log_rec->prev_lsn_ = txn->get_prev_lsn();
        txn->set_prev_lsn(log_manager->add_log_to_buffer(log_rec));
    }

    // 释放所有锁
    auto lock_set = txn->get_lock_set();
    for (auto& lock_data_id : *lock_set) {
        lock_manager_->unlock(txn, lock_data_id);
    }
    lock_set->clear();

    // 清空写集
    auto write_set = txn->get_write_set();
    for (auto* wr : *write_set) {
        delete wr;
    }
    write_set->clear();

    // 更新事务状态
    txn->set_state(TransactionState::COMMITTED);
}

void TransactionManager::abort(Transaction * txn, LogManager *log_manager) {
    // 1. 回滚所有写操作（逆序回滚）
    auto write_set = txn->get_write_set();
    while (!write_set->empty()) {
        auto* wr = write_set->back();
        write_set->pop_back();

        auto fh = sm_manager_->fhs_.at(wr->GetTableName()).get();

        switch (wr->GetWriteType()) {
            case WType::INSERT_TUPLE:
                // 回滚插入：删除已插入的记录
                fh->delete_record(wr->GetRid(), nullptr);
                break;
            case WType::DELETE_TUPLE:
                // 回滚删除：重新插入被删除的记录
                fh->insert_record(wr->GetRid(), wr->GetRecord().data);
                break;
            case WType::UPDATE_TUPLE:
                // 回滚更新：恢复旧值
                fh->update_record(wr->GetRid(), wr->GetRecord().data, nullptr);
                break;
        }
        delete wr;
    }

    // 2. 释放所有锁
    auto lock_set = txn->get_lock_set();
    for (auto& lock_data_id : *lock_set) {
        lock_manager_->unlock(txn, lock_data_id);
    }
    lock_set->clear();

    // 3. 写 ABORT 日志
    if (log_manager != nullptr) {
        auto log_rec = new AbortLogRecord();
        log_rec->log_tid_ = txn->get_transaction_id();
        log_rec->prev_lsn_ = txn->get_prev_lsn();
        txn->set_prev_lsn(log_manager->add_log_to_buffer(log_rec));
    }

    // 4. 更新事务状态
    txn->set_state(TransactionState::ABORTED);
}


bool TransactionManager::UpdateUndoLink(Rid rid, std::optional<UndoLink> prev_link,
                    std::function<bool(std::optional<UndoLink>)> &&check) {
    // MVCC helper - stub for now
    return false;
}

bool TransactionManager::UpdateVersionLink(Rid rid, std::optional<VersionUndoLink> prev_version,
                       std::function<bool(std::optional<VersionUndoLink>)> &&check) {
    // MVCC helper - stub for now
    return false;
}

std::optional<UndoLink> TransactionManager::GetUndoLink(Rid rid) {
    return std::nullopt;
}

std::optional<VersionUndoLink> TransactionManager::GetVersionLink(Rid rid) {
    return std::nullopt;
}

std::optional<UndoLog> TransactionManager::GetUndoLogOptional(UndoLink link) {
    return std::nullopt;
}

UndoLog TransactionManager::GetUndoLog(UndoLink link) {
    throw RMDBError("Not implemented");
}

timestamp_t TransactionManager::GetWatermark() {
    return 0;
}

void TransactionManager::GarbageCollection() {
    // Not implemented yet
}

