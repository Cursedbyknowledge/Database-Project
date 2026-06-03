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
#include <algorithm>
#include <set>

std::unordered_map<txn_id_t, Transaction *> TransactionManager::txn_map = {};

// Global timestamp counter
static std::atomic<timestamp_t> global_ts{0};
// Track active write sets per record for conflict detection
static std::mutex write_set_mutex;
// rid -> set of active txn_ids that have written to this record (not yet committed)
static std::unordered_map<int64_t, std::set<txn_id_t>> record_writers;  // key = (page_no << 32) | slot_no
// For SSI: track committed writes that might create rw-dependencies
struct CommittedWrite {
    txn_id_t txn_id;
    timestamp_t commit_ts;
    int64_t rid_key;
};
static std::vector<CommittedWrite> committed_writes;  // simplified; production would use a better structure
static std::mutex committed_writes_mutex;

static inline int64_t rid_to_key(const Rid &rid) {
    return ((int64_t)rid.page_no << 32) | (rid.slot_no & 0xFFFFFFFF);
}

/**
 * @description: 事务的开始方法
 */
Transaction * TransactionManager::begin(Transaction* txn, LogManager* log_manager) {
    if (txn == nullptr) {
        txn_id_t txn_id = next_txn_id_.fetch_add(1);
        txn = new Transaction(txn_id);
        txn->set_state(TransactionState::GROWING);
    } else {
        txn->set_state(TransactionState::GROWING);
    }
    
    // Assign start timestamp
    timestamp_t start_ts = next_timestamp_.fetch_add(1);
    txn->set_start_ts(start_ts);
    txn->read_ts_ = start_ts;  // For MVCC, read_ts = start_ts
    
    // Register in global map
    std::unique_lock<std::mutex> lock(latch_);
    txn_map[txn->get_transaction_id()] = txn;
    running_txns_.AddTxn(start_ts);
    lock.unlock();
    
    // Record begin log
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
    
    // Assign commit timestamp
    timestamp_t commit_ts = next_timestamp_.fetch_add(1);
    txn->commit_ts_ = commit_ts;
    
    // Clear write set tracking
    {
        std::unique_lock<std::mutex> lock(write_set_mutex);
        auto write_set = txn->get_write_set();
        for (auto *wr : *write_set) {
            int64_t key = rid_to_key(wr->GetRid());
            record_writers[key].erase(txn->get_transaction_id());
            if (record_writers[key].empty()) {
                record_writers.erase(key);
            }
        }
        
        // Record committed writes for SSI
        std::unique_lock<std::mutex> cl(committed_writes_mutex);
        for (auto *wr : *write_set) {
            CommittedWrite cw;
            cw.txn_id = txn->get_transaction_id();
            cw.commit_ts = commit_ts;
            cw.rid_key = rid_to_key(wr->GetRid());
            committed_writes.push_back(cw);
        }
    }
    
    // Record commit log
    if (log_manager != nullptr) {
        CommitLogRecord *commit_log = new CommitLogRecord(txn->get_transaction_id());
        log_manager->add_log_to_buffer(commit_log);
        delete commit_log;
        log_manager->flush_log_to_disk();
    }
    
    // Release locks
    if (lock_manager_ != nullptr) {
        auto lock_set = txn->get_lock_set();
        for (auto &lock_id : *lock_set) {
            lock_manager_->unlock(txn, lock_id);
        }
    }
    
    txn->set_state(TransactionState::COMMITTED);
    
    // Update watermark
    std::unique_lock<std::mutex> lock(latch_);
    txn_map.erase(txn->get_transaction_id());
    running_txns_.RemoveTxn(txn->get_start_ts());
    last_commit_ts_ = commit_ts;
    lock.unlock();
}

/**
 * @description: 事务的终止（回滚）方法
 */
void TransactionManager::abort(Transaction * txn, LogManager *log_manager) {
    if (txn == nullptr) return;
    
    // Rollback writes (reverse order)
    auto write_set = txn->get_write_set();
    while (!write_set->empty()) {
        WriteRecord *wr = write_set->back();
        write_set->pop_back();
        
        auto &fhs = sm_manager_->fhs_;
        if (fhs.find(wr->GetTableName()) != fhs.end()) {
            auto fh = fhs[wr->GetTableName()].get();
            switch (wr->GetWriteType()) {
                case WType::INSERT_TUPLE:
                    fh->delete_record(wr->GetRid(), nullptr);
                    break;
                case WType::DELETE_TUPLE:
                    fh->insert_record(wr->GetRid(), wr->GetRecord().data);
                    break;
                case WType::UPDATE_TUPLE:
                    fh->update_record(wr->GetRid(), wr->GetRecord().data, nullptr);
                    break;
            }
        }
        delete wr;
    }
    
    // Clear write set tracking
    {
        std::unique_lock<std::mutex> lock(write_set_mutex);
        for (auto &entry : record_writers) {
            entry.second.erase(txn->get_transaction_id());
        }
    }
    
    // Clear SSI dependency graph: 从所有活跃事务中移除本事务的依赖边
    {
        std::unique_lock<std::mutex> lock(latch_);
        txn_id_t my_tid = txn->get_transaction_id();
        for (auto &[tid, other_txn] : txn_map) {
            if (tid == my_tid) continue;
            // 从其他事务中删除指向本事务的边
            other_txn->remove_in_edge(my_tid);
            other_txn->remove_out_edge(my_tid);
        }
    }
    
    // Release locks
    if (lock_manager_ != nullptr) {
        auto lock_set = txn->get_lock_set();
        for (auto &lock_id : *lock_set) {
            lock_manager_->unlock(txn, lock_id);
        }
    }
    
    // Record abort log
    if (log_manager != nullptr) {
        AbortLogRecord *abort_log = new AbortLogRecord(txn->get_transaction_id());
        log_manager->add_log_to_buffer(abort_log);
        delete abort_log;
        log_manager->flush_log_to_disk();
    }
    
    txn->set_state(TransactionState::ABORTED);
    
    std::unique_lock<std::mutex> lock(latch_);
    txn_map.erase(txn->get_transaction_id());
    running_txns_.RemoveTxn(txn->get_start_ts());
    lock.unlock();
}

// ========== MVCC Write-Write Conflict Detection ==========

/**
 * Check if the given record is being written by another active transaction.
 * Returns true if conflict detected (another txn is writing this record).
 */
bool TransactionManager::check_write_conflict(Transaction *txn, const Rid &rid) {
    if (concurrency_mode_ != ConcurrencyMode::TWO_PHASE_LOCKING) {
        int64_t key = rid_to_key(rid);
        std::unique_lock<std::mutex> lock(write_set_mutex);
        auto it = record_writers.find(key);
        if (it != record_writers.end()) {
            for (auto other_tid : it->second) {
                if (other_tid != txn->get_transaction_id()) {
                    // Another transaction is writing this record
                    return true;
                }
            }
        }
        // Mark this transaction as writing this record
        record_writers[key].insert(txn->get_transaction_id());
    }
    return false;
}

/**
 * Record that a transaction has written to a record (for conflict detection).
 */
void TransactionManager::record_write(Transaction *txn, const Rid &rid) {
    int64_t key = rid_to_key(rid);
    std::unique_lock<std::mutex> lock(write_set_mutex);
    record_writers[key].insert(txn->get_transaction_id());
}

/**
 * Check if a record has been modified by an uncommitted transaction.
 * Used by read path to detect dirty reads under SI/SER.
 * The caller should treat this as a visibility issue:
 * - Under SI: the record is not yet committed, so read the old version
 * - Under SER: this may create a rw-dependency
 */
bool TransactionManager::is_record_dirty_by_other(Transaction *txn, const Rid &rid) {
    int64_t key = rid_to_key(rid);
    std::unique_lock<std::mutex> lock(write_set_mutex);
    auto it = record_writers.find(key);
    if (it != record_writers.end()) {
        for (auto other_tid : it->second) {
            if (other_tid != txn->get_transaction_id()) {
                // Check if the other transaction is still active (uncommitted)
                std::unique_lock<std::mutex> lk(latch_);
                auto mit = txn_map.find(other_tid);
                if (mit != txn_map.end() && mit->second->get_state() == TransactionState::GROWING) {
                    return true;
                }
            }
        }
    }
    return false;
}

// ========== SSI Dependency Tracking ==========

/**
 * Check if reading this record creates a rw-dependency for SSI.
 * reader ->rw-> writer: reader's snapshot doesn't include writer's changes.
 * Sets reader's rw_dependency_in_ and writer's rw_dependency_out_.
 * If reader becomes a pivot (both in and out), signal dangerous structure.
 */
bool TransactionManager::check_rw_dependency(Transaction *txn, const Rid &rid, bool is_range_scan) {
    if (txn->get_isolation_level() != IsolationLevel::SERIALIZABLE) return false;
    
    int64_t key = rid_to_key(rid);
    timestamp_t my_start = txn->get_start_ts();
    bool found_dep = false;
    
    // Check committed writes newer than our snapshot
    {
        std::unique_lock<std::mutex> lock(committed_writes_mutex);
        for (auto &cw : committed_writes) {
            if (cw.rid_key == key || is_range_scan) {
                if (cw.commit_ts > my_start && cw.txn_id != txn->get_transaction_id()) {
                    // reader(us) ->rw-> writer(cw.txn_id)
                    // Writer may have already completed; still record dependency on us
                    txn->add_out_edge(cw.txn_id);
                    found_dep = true;
                }
            }
        }
    }
    
    // Check active writers
    {
        std::unique_lock<std::mutex> wl(write_set_mutex);
        auto it = record_writers.find(key);
        if (it != record_writers.end()) {
            for (auto other_tid : it->second) {
                if (other_tid != txn->get_transaction_id()) {
                    Transaction *other = nullptr;
                    {
                        std::unique_lock<std::mutex> lk(latch_);
                        auto mit = txn_map.find(other_tid);
                        if (mit != txn_map.end()) other = mit->second;
                    }
                    if (other != nullptr && other->get_state() == TransactionState::GROWING) {
                        // reader(us) ->rw-> writer(other)
                        // other wrote data that we read → other gets in_edge from us
                        // we read data that other wrote → we get out_edge to other
                        other->add_in_edge(txn->get_transaction_id());
                        txn->add_out_edge(other_tid);
                        found_dep = true;
                        // Check if other is now a pivot
                        if (other->is_ssi_pivot()) return true;
                    }
                }
            }
        }
    }
    
    // After reading, signal if WE became a pivot
    if (found_dep && txn->is_ssi_pivot()) return true;
    return false;
}

/**
 * Check if writing creates a rw-dependency for SSI that forms a dangerous structure.
 * When T writes, check if any other txn has read data that T is now modifying.
 * reader ->rw-> writer(us)
 */
bool TransactionManager::check_dangerous_structure(Transaction *txn, const Rid &rid) {
    if (txn->get_isolation_level() != IsolationLevel::SERIALIZABLE) return false;
    
    int64_t key = rid_to_key(rid);
    bool found_dep = false;
    
    std::unique_lock<std::mutex> lock(latch_);
    for (auto &[tid, other_txn] : txn_map) {
        if (tid == txn->get_transaction_id()) continue;
        if (other_txn->get_state() != TransactionState::GROWING) continue;
        if (other_txn->get_isolation_level() != IsolationLevel::SERIALIZABLE) continue;
        
        // Check if other_txn has read the record we're now writing
        auto read_set = other_txn->get_read_set();
        for (auto &read_rid : *read_set) {
            if (rid_to_key(read_rid) == key) {
                // other_txn(read) ->rw-> txn(us, write)
                // other_txn read data we're now writing → other depends on us
                other_txn->add_out_edge(txn->get_transaction_id());
                txn->add_in_edge(tid);
                found_dep = true;
                
                // Check if either transaction became a pivot
                if (other_txn->is_ssi_pivot()) return true;
                if (txn->is_ssi_pivot()) return true;
                break;
            }
        }
        if (found_dep) break;  // One dependency edge is enough per write
    }
    
    return false;  // No dangerous structure yet
}

// ========== MVCC Stubs (not needed for basic functionality) ==========
bool TransactionManager::UpdateUndoLink(Rid rid, std::optional<UndoLink> prev_link,
                    std::function<bool(std::optional<UndoLink>)> &&check) { return false; }
bool TransactionManager::UpdateVersionLink(Rid rid, std::optional<VersionUndoLink> prev_version,
                   std::function<bool(std::optional<VersionUndoLink>)> &&check) { return false; }
std::optional<UndoLink> TransactionManager::GetUndoLink(Rid rid) { return std::nullopt; }
std::optional<VersionUndoLink> TransactionManager::GetVersionLink(Rid rid) { return std::nullopt; }
std::optional<UndoLog> TransactionManager::GetUndoLogOptional(UndoLink link) { return std::nullopt; }
UndoLog TransactionManager::GetUndoLog(UndoLink link) { return UndoLog(); }
timestamp_t TransactionManager::GetWatermark() { return running_txns_.GetWatermark(); }
void TransactionManager::GarbageCollection() {}
