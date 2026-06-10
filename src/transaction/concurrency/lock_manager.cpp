/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "lock_manager.h"
#include <algorithm>

bool LockManager::is_compatible(LockMode request, GroupLockMode granted) {
    switch (request) {
        case LockMode::SHARED:
            return granted == GroupLockMode::NON_LOCK || granted == GroupLockMode::IS ||
                   granted == GroupLockMode::S;
        case LockMode::EXLUCSIVE:
            return granted == GroupLockMode::NON_LOCK;
        case LockMode::INTENTION_SHARED:
            return granted != GroupLockMode::X;
        case LockMode::INTENTION_EXCLUSIVE:
            return granted == GroupLockMode::NON_LOCK || granted == GroupLockMode::IS ||
                   granted == GroupLockMode::IX;
        case LockMode::S_IX:
            return granted == GroupLockMode::NON_LOCK || granted == GroupLockMode::IS;
        default:
            return false;
    }
}

LockManager::GroupLockMode LockManager::compute_group_mode(const std::list<LockRequest>& requests) {
    bool has_x = false, has_s = false, has_ix = false, has_is = false, has_six = false;
    for (auto& req : requests) {
        if (!req.granted_) continue;
        switch (req.lock_mode_) {
            case LockMode::SHARED:               has_s = true; break;
            case LockMode::EXLUCSIVE:             has_x = true; break;
            case LockMode::INTENTION_SHARED:      has_is = true; break;
            case LockMode::INTENTION_EXCLUSIVE:   has_ix = true; break;
            case LockMode::S_IX:                  has_six = true; break;
        }
    }
    if (has_x)  return GroupLockMode::X;
    if (has_six) return GroupLockMode::SIX;
    if (has_s && has_ix) return GroupLockMode::SIX;
    if (has_s)  return GroupLockMode::S;
    if (has_ix) return GroupLockMode::IX;
    if (has_is) return GroupLockMode::IS;
    return GroupLockMode::NON_LOCK;
}

bool LockManager::already_holds_lock(const std::list<LockRequest>& requests, txn_id_t txn_id, LockMode request_mode) {
    for (auto& req : requests) {
        if (req.txn_id_ != txn_id || !req.granted_) continue;
        if (req.lock_mode_ == LockMode::EXLUCSIVE) return true;
        if (req.lock_mode_ == LockMode::S_IX &&
            (request_mode == LockMode::SHARED || request_mode == LockMode::INTENTION_EXCLUSIVE ||
             request_mode == LockMode::INTENTION_SHARED)) return true;
        if (req.lock_mode_ == LockMode::SHARED && request_mode == LockMode::INTENTION_SHARED) return true;
        if (req.lock_mode_ == LockMode::INTENTION_EXCLUSIVE &&
            request_mode == LockMode::INTENTION_SHARED) return true;
        if (req.lock_mode_ == request_mode) return true;
    }
    return false;
}

bool LockManager::acquire_lock(Transaction* txn, const LockDataId& lock_data_id, LockMode lock_mode) {
    auto& queue = lock_table_[lock_data_id];

    if (already_holds_lock(queue.request_queue_, txn->get_transaction_id(), lock_mode)) {
        txn->get_lock_set()->insert(lock_data_id);
        return true;
    }

    if (!is_compatible(lock_mode, queue.group_lock_mode_)) {
        return false;
    }

    queue.request_queue_.emplace_back(txn->get_transaction_id(), lock_mode);
    queue.request_queue_.back().granted_ = true;
    queue.group_lock_mode_ = compute_group_mode(queue.request_queue_);
    txn->get_lock_set()->insert(lock_data_id);
    return true;
}

bool LockManager::lock_shared_on_record(Transaction* txn, const Rid& rid, int tab_fd) {
    std::scoped_lock lock(latch_);
    LockDataId lock_data_id(tab_fd, rid, LockDataType::RECORD);
    return acquire_lock(txn, lock_data_id, LockMode::SHARED);
}

bool LockManager::lock_exclusive_on_record(Transaction* txn, const Rid& rid, int tab_fd) {
    std::scoped_lock lock(latch_);
    LockDataId lock_data_id(tab_fd, rid, LockDataType::RECORD);
    return acquire_lock(txn, lock_data_id, LockMode::EXLUCSIVE);
}

bool LockManager::lock_shared_on_table(Transaction* txn, int tab_fd) {
    std::scoped_lock lock(latch_);
    LockDataId lock_data_id(tab_fd, LockDataType::TABLE);
    return acquire_lock(txn, lock_data_id, LockMode::SHARED);
}

bool LockManager::lock_exclusive_on_table(Transaction* txn, int tab_fd) {
    std::scoped_lock lock(latch_);
    LockDataId lock_data_id(tab_fd, LockDataType::TABLE);
    return acquire_lock(txn, lock_data_id, LockMode::EXLUCSIVE);
}

bool LockManager::lock_IS_on_table(Transaction* txn, int tab_fd) {
    std::scoped_lock lock(latch_);
    LockDataId lock_data_id(tab_fd, LockDataType::TABLE);
    return acquire_lock(txn, lock_data_id, LockMode::INTENTION_SHARED);
}

bool LockManager::lock_IX_on_table(Transaction* txn, int tab_fd) {
    std::scoped_lock lock(latch_);
    LockDataId lock_data_id(tab_fd, LockDataType::TABLE);
    return acquire_lock(txn, lock_data_id, LockMode::INTENTION_EXCLUSIVE);
}

bool LockManager::unlock(Transaction* txn, LockDataId lock_data_id) {
    std::scoped_lock lock(latch_);
    auto it = lock_table_.find(lock_data_id);
    if (it == lock_table_.end()) return true;

    auto& queue = it->second;
    queue.request_queue_.remove_if([&](const LockRequest& req) {
        return req.txn_id_ == txn->get_transaction_id();
    });

    queue.group_lock_mode_ = compute_group_mode(queue.request_queue_);
    if (queue.request_queue_.empty()) {
        lock_table_.erase(it);
    }
    txn->get_lock_set()->erase(lock_data_id);
    return true;
}
