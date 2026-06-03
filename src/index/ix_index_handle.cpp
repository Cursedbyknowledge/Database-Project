/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "ix_index_handle.h"
#include <algorithm>
#include "ix_scan.h"

int IxNodeHandle::lower_bound(const char *target) const {
    int lo = 0, hi = page_hdr->num_key;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (ix_compare(get_key(mid), target, file_hdr->col_types_, file_hdr->col_lens_) < 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

int IxNodeHandle::upper_bound(const char *target) const {
    int lo = 1, hi = page_hdr->num_key;
    while (lo < hi) {
        int mid = lo + (hi - lo) / 2;
        if (ix_compare(get_key(mid), target, file_hdr->col_types_, file_hdr->col_lens_) <= 0) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return lo;
}

bool IxNodeHandle::leaf_lookup(const char *key, Rid **value) {
    int pos = lower_bound(key);
    if (pos < page_hdr->num_key &&
        ix_compare(get_key(pos), key, file_hdr->col_types_, file_hdr->col_lens_) == 0) {
        *value = get_rid(pos);
        return true;
    }
    return false;
}

page_id_t IxNodeHandle::internal_lookup(const char *key) {
    int pos = upper_bound(key);
    return value_at(pos - 1);
}

void IxNodeHandle::insert_pairs(int pos, const char *key, const Rid *rid, int n) {
    assert(pos >= 0 && pos <= page_hdr->num_key);
    int move_cnt = page_hdr->num_key - pos;
    if (move_cnt > 0) {
        memmove(keys + (pos + n) * file_hdr->col_tot_len_,
                keys + pos * file_hdr->col_tot_len_,
                move_cnt * file_hdr->col_tot_len_);
        memmove(rids + pos + n, rids + pos, move_cnt * sizeof(Rid));
    }
    for (int i = 0; i < n; i++) {
        memcpy(keys + (pos + i) * file_hdr->col_tot_len_,
               key + i * file_hdr->col_tot_len_,
               file_hdr->col_tot_len_);
        rids[pos + i] = rid[i];
    }
    page_hdr->num_key += n;
}

int IxNodeHandle::insert(const char *key, const Rid &value) {
    int pos = lower_bound(key);
    if (pos < page_hdr->num_key &&
        ix_compare(get_key(pos), key, file_hdr->col_types_, file_hdr->col_lens_) == 0) {
        return page_hdr->num_key;
    }
    insert_pair(pos, key, value);
    return page_hdr->num_key;
}

void IxNodeHandle::erase_pair(int pos) {
    assert(pos >= 0 && pos < page_hdr->num_key);
    int move_cnt = page_hdr->num_key - pos - 1;
    if (move_cnt > 0) {
        memmove(keys + pos * file_hdr->col_tot_len_,
                keys + (pos + 1) * file_hdr->col_tot_len_,
                move_cnt * file_hdr->col_tot_len_);
        memmove(rids + pos, rids + pos + 1, move_cnt * sizeof(Rid));
    }
    page_hdr->num_key--;
}

int IxNodeHandle::remove(const char *key) {
    int pos = lower_bound(key);
    if (pos < page_hdr->num_key &&
        ix_compare(get_key(pos), key, file_hdr->col_types_, file_hdr->col_lens_) == 0) {
        erase_pair(pos);
    }
    return page_hdr->num_key;
}

IxIndexHandle::IxIndexHandle(DiskManager *disk_manager, BufferPoolManager *buffer_pool_manager, int fd)
    : disk_manager_(disk_manager), buffer_pool_manager_(buffer_pool_manager), fd_(fd) {
    char* buf = new char[PAGE_SIZE];
    memset(buf, 0, PAGE_SIZE);
    disk_manager_->read_page(fd, IX_FILE_HDR_PAGE, buf, PAGE_SIZE);
    file_hdr_ = new IxFileHdr();
    file_hdr_->deserialize(buf);
    delete[] buf;
    
    int now_page_no = disk_manager_->get_fd2pageno(fd);
    disk_manager_->set_fd2pageno(fd, now_page_no + 1);
}

std::pair<IxNodeHandle *, bool> IxIndexHandle::find_leaf_page(const char *key, Operation operation,
                                                            Transaction *transaction, bool find_first) {
    if (is_empty()) {
        return std::make_pair(nullptr, false);
    }
    IxNodeHandle *node = fetch_node(file_hdr_->root_page_);
    while (!node->is_leaf_page()) {
        page_id_t child_page = node->internal_lookup(key);
        IxNodeHandle *child = fetch_node(child_page);
        buffer_pool_manager_->unpin_page(node->get_page_id(), false);
        delete node;
        node = child;
    }
    return std::make_pair(node, false);
}

bool IxIndexHandle::get_value(const char *key, std::vector<Rid> *result, Transaction *transaction) {
    auto [leaf, root_is_latched] = find_leaf_page(key, Operation::FIND, transaction, false);
    if (leaf == nullptr) return false;
    Rid *rid = nullptr;
    bool found = leaf->leaf_lookup(key, &rid);
    if (found) {
        result->push_back(*rid);
    }
    buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
    delete leaf;
    return found;
}

IxNodeHandle *IxIndexHandle::split(IxNodeHandle *node) {
    IxNodeHandle *new_node = create_node();
    new_node->page_hdr->is_leaf = node->page_hdr->is_leaf;
    new_node->page_hdr->parent = node->page_hdr->parent;

    int total = node->get_size();
    int mid = total / 2;
    int move_cnt = total - mid;

    new_node->insert_pairs(0, node->get_key(mid), node->get_rid(mid), move_cnt);
    node->set_size(mid);

    if (node->is_leaf_page()) {
        new_node->set_prev_leaf(node->get_page_no());
        new_node->set_next_leaf(node->get_next_leaf());
        node->set_next_leaf(new_node->get_page_no());

        if (new_node->get_next_leaf() != IX_LEAF_HEADER_PAGE) {
            IxNodeHandle *next = fetch_node(new_node->get_next_leaf());
            next->set_prev_leaf(new_node->get_page_no());
            buffer_pool_manager_->unpin_page(next->get_page_id(), true);
            delete next;
        }

        if (file_hdr_->last_leaf_ == node->get_page_no()) {
            file_hdr_->last_leaf_ = new_node->get_page_no();
        }
    } else {
        for (int i = 0; i < new_node->get_size(); i++) {
            maintain_child(new_node, i);
        }
    }
    return new_node;
}

void IxIndexHandle::insert_into_parent(IxNodeHandle *old_node, const char *key, IxNodeHandle *new_node,
                                     Transaction *transaction) {
    if (old_node->is_root_page()) {
        IxNodeHandle *new_root = create_node();
        new_root->page_hdr->is_leaf = false;
        new_root->page_hdr->parent = IX_NO_PAGE;

        Rid old_rid = {.page_no = old_node->get_page_no(), .slot_no = -1};
        Rid new_rid = {.page_no = new_node->get_page_no(), .slot_no = -1};

        new_root->insert_pair(0, old_node->get_key(0), old_rid);
        new_root->insert_pair(1, key, new_rid);

        old_node->set_parent_page_no(new_root->get_page_no());
        new_node->set_parent_page_no(new_root->get_page_no());

        update_root_page_no(new_root->get_page_no());

        buffer_pool_manager_->unpin_page(new_root->get_page_id(), true);
        delete new_root;
    } else {
        IxNodeHandle *parent = fetch_node(old_node->get_parent_page_no());
        int rank = parent->find_child(old_node);

        Rid rid = {.page_no = new_node->get_page_no(), .slot_no = -1};
        parent->insert_pair(rank + 1, key, rid);

        if (parent->get_size() >= parent->get_max_size()) {
            IxNodeHandle *new_parent = split(parent);
            const char *new_key = new_parent->get_key(0);
            insert_into_parent(parent, new_key, new_parent, transaction);
            buffer_pool_manager_->unpin_page(new_parent->get_page_id(), true);
            delete new_parent;
        }

        buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
        delete parent;
    }
}

page_id_t IxIndexHandle::insert_entry(const char *key, const Rid &value, Transaction *transaction) {
    if (is_empty()) {
        IxNodeHandle *root = fetch_node(file_hdr_->root_page_);
        root->insert(key, value);
        buffer_pool_manager_->unpin_page(root->get_page_id(), true);
        page_id_t res = root->get_page_no();
        delete root;
        return res;
    }

    auto [leaf, root_latched] = find_leaf_page(key, Operation::INSERT, transaction, false);

    int old_size = leaf->get_size();
    leaf->insert(key, value);
    int leaf_page_no = leaf->get_page_no();

    if (leaf->get_size() == old_size) {
        buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
        delete leaf;
        return leaf_page_no;
    }

    if (leaf->get_size() >= leaf->get_max_size()) {
        IxNodeHandle *new_leaf = split(leaf);
        const char *new_key = new_leaf->get_key(0);
        insert_into_parent(leaf, new_key, new_leaf, transaction);
        buffer_pool_manager_->unpin_page(new_leaf->get_page_id(), true);
        delete new_leaf;
    }

    buffer_pool_manager_->unpin_page(leaf->get_page_id(), true);
    delete leaf;
    return leaf_page_no;
}

bool IxIndexHandle::delete_entry(const char *key, Transaction *transaction) {
    auto [leaf, root_latched] = find_leaf_page(key, Operation::DELETE, transaction, false);
    if (leaf == nullptr) return false;

    int old_size = leaf->get_size();
    leaf->remove(key);

    if (old_size == leaf->get_size()) {
        buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
        delete leaf;
        return false;
    }

    bool root_is_latched_out = false;
    coalesce_or_redistribute(leaf, transaction, &root_is_latched_out);

    buffer_pool_manager_->unpin_page(leaf->get_page_id(), true);
    delete leaf;
    return true;
}

bool IxIndexHandle::coalesce_or_redistribute(IxNodeHandle *node, Transaction *transaction, bool *root_is_latched) {
    if (node->is_root_page()) {
        return adjust_root(node);
    }

    if (node->get_size() >= node->get_min_size()) {
        maintain_parent(node);
        return false;
    }

    IxNodeHandle *parent = fetch_node(node->get_parent_page_no());
    int rank = parent->find_child(node);

    IxNodeHandle *neighbor = nullptr;
    if (rank > 0) {
        neighbor = fetch_node(parent->value_at(rank - 1));
    } else {
        neighbor = fetch_node(parent->value_at(rank + 1));
    }

    bool res = false;
    if (node->get_size() + neighbor->get_size() >= node->get_max_size()) {
        redistribute(neighbor, node, parent, rank);
        res = false;
    } else {
        res = coalesce(&neighbor, &node, &parent, rank, transaction, root_is_latched);
    }

    buffer_pool_manager_->unpin_page(neighbor->get_page_id(), true);
    delete neighbor;
    buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
    delete parent;
    return res;
}

bool IxIndexHandle::adjust_root(IxNodeHandle *old_root_node) {
    if (old_root_node->is_leaf_page()) {
        return false;
    }

    if (old_root_node->get_size() == 1) {
        page_id_t child_page = old_root_node->value_at(0);
        IxNodeHandle *child = fetch_node(child_page);
        child->set_parent_page_no(IX_NO_PAGE);
        update_root_page_no(child_page);
        buffer_pool_manager_->unpin_page(child->get_page_id(), true);
        delete child;
        return true;
    }
    return false;
}

void IxIndexHandle::redistribute(IxNodeHandle *neighbor_node, IxNodeHandle *node, IxNodeHandle *parent, int index) {
    if (index > 0) {
        int neighbor_last = neighbor_node->get_size() - 1;
        node->insert_pair(0, neighbor_node->get_key(neighbor_last), *neighbor_node->get_rid(neighbor_last));
        neighbor_node->erase_pair(neighbor_last);
        parent->set_key(index, node->get_key(0));
        if (!node->is_leaf_page()) {
            maintain_child(node, 0);
        }
    } else {
        node->insert_pair(node->get_size(), neighbor_node->get_key(0), *neighbor_node->get_rid(0));
        neighbor_node->erase_pair(0);
        if (neighbor_node->get_size() > 0) {
            parent->set_key(1, neighbor_node->get_key(0));
        }
        if (!node->is_leaf_page()) {
            maintain_child(node, node->get_size() - 1);
        }
    }
}

bool IxIndexHandle::coalesce(IxNodeHandle **neighbor_node, IxNodeHandle **node, IxNodeHandle **parent, int index,
                             Transaction *transaction, bool *root_is_latched) {
    if (index == 0) {
        std::swap(*neighbor_node, *node);
        index = 1;
    }

    IxNodeHandle *left = *neighbor_node;
    IxNodeHandle *right = *node;
    IxNodeHandle *par = *parent;

    int left_size = left->get_size();
    left->insert_pairs(left_size, right->get_key(0), right->get_rid(0), right->get_size());

    if (right->is_leaf_page()) {
        left->set_next_leaf(right->get_next_leaf());
        if (right->get_next_leaf() != IX_LEAF_HEADER_PAGE) {
            IxNodeHandle *next = fetch_node(right->get_next_leaf());
            next->set_prev_leaf(left->get_page_no());
            buffer_pool_manager_->unpin_page(next->get_page_id(), true);
            delete next;
        }
        if (file_hdr_->last_leaf_ == right->get_page_no()) {
            file_hdr_->last_leaf_ = left->get_page_no();
        }
    } else {
        for (int i = left_size; i < left->get_size(); i++) {
            maintain_child(left, i);
        }
    }

    par->erase_pair(index);
    release_node_handle(*right);

    if (par->is_root_page()) {
        return adjust_root(par);
    }
    if (par->get_size() < par->get_min_size()) {
        return coalesce_or_redistribute(par, transaction, root_is_latched);
    }
    return false;
}

Rid IxIndexHandle::get_rid(const Iid &iid) const {
    IxNodeHandle *node = fetch_node(iid.page_no);
    if (iid.slot_no >= node->get_size()) {
        buffer_pool_manager_->unpin_page(node->get_page_id(), false);
        delete node;
        throw IndexEntryNotFoundError();
    }
    Rid res = *node->get_rid(iid.slot_no);
    buffer_pool_manager_->unpin_page(node->get_page_id(), false);
    delete node;
    return res;
}

Iid IxIndexHandle::lower_bound(const char *key) {
    auto [leaf, _] = find_leaf_page(key, Operation::FIND, nullptr, false);
    if (leaf == nullptr) return leaf_end();
    int pos = leaf->lower_bound(key);
    Iid iid = {.page_no = leaf->get_page_no(), .slot_no = pos};
    buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
    delete leaf;
    return iid;
}

Iid IxIndexHandle::upper_bound(const char *key) {
    auto [leaf, _] = find_leaf_page(key, Operation::FIND, nullptr, false);
    if (leaf == nullptr) return leaf_end();
    int pos = leaf->upper_bound(key);
    Iid iid = {.page_no = leaf->get_page_no(), .slot_no = pos};
    buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
    delete leaf;
    return iid;
}

Iid IxIndexHandle::leaf_end() const {
    IxNodeHandle *node = fetch_node(file_hdr_->last_leaf_);
    Iid iid = {.page_no = file_hdr_->last_leaf_, .slot_no = node->get_size()};
    buffer_pool_manager_->unpin_page(node->get_page_id(), false);
    delete node;
    return iid;
}

Iid IxIndexHandle::leaf_begin() const {
    Iid iid = {.page_no = file_hdr_->first_leaf_, .slot_no = 0};
    return iid;
}

IxNodeHandle *IxIndexHandle::fetch_node(int page_no) const {
    Page *page = buffer_pool_manager_->fetch_page(PageId{fd_, page_no});
    IxNodeHandle *node = new IxNodeHandle(file_hdr_, page);
    return node;
}

IxNodeHandle *IxIndexHandle::create_node() {
    IxNodeHandle *node;
    file_hdr_->num_pages_++;
    PageId new_page_id = {.fd = fd_, .page_no = INVALID_PAGE_ID};
    Page *page = buffer_pool_manager_->new_page(&new_page_id);
    node = new IxNodeHandle(file_hdr_, page);
    return node;
}

void IxIndexHandle::maintain_parent(IxNodeHandle *node) {
    IxNodeHandle *curr = node;
    while (curr->get_parent_page_no() != IX_NO_PAGE) {
        IxNodeHandle *parent = fetch_node(curr->get_parent_page_no());
        int rank = parent->find_child(curr);
        char *parent_key = parent->get_key(rank);
        char *child_first_key = curr->get_key(0);
        if (memcmp(parent_key, child_first_key, file_hdr_->col_tot_len_) == 0) {
            buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
            delete parent;
            break;
        }
        memcpy(parent_key, child_first_key, file_hdr_->col_tot_len_);
        buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
        if (curr != node) delete curr;
        curr = parent;
    }
    if (curr != node) delete curr;
}

void IxIndexHandle::erase_leaf(IxNodeHandle *leaf) {
    assert(leaf->is_leaf_page());
    IxNodeHandle *prev = fetch_node(leaf->get_prev_leaf());
    prev->set_next_leaf(leaf->get_next_leaf());
    buffer_pool_manager_->unpin_page(prev->get_page_id(), true);
    delete prev;

    IxNodeHandle *next = fetch_node(leaf->get_next_leaf());
    next->set_prev_leaf(leaf->get_prev_leaf());
    buffer_pool_manager_->unpin_page(next->get_page_id(), true);
    delete next;
}

void IxIndexHandle::release_node_handle(IxNodeHandle &node) {
    file_hdr_->num_pages_--;
}

void IxIndexHandle::maintain_child(IxNodeHandle *node, int child_idx) {
    if (!node->is_leaf_page()) {
        int child_page_no = node->value_at(child_idx);
        IxNodeHandle *child = fetch_node(child_page_no);
        child->set_parent_page_no(node->get_page_no());
        buffer_pool_manager_->unpin_page(child->get_page_id(), true);
        delete child;
    }
}
