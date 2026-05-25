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
    int lo = 0, hi = page_hdr->num_key;
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
    if (pos < page_hdr->num_key && ix_compare(get_key(pos), key, file_hdr->col_types_, file_hdr->col_lens_) == 0) {
        *value = get_rid(pos);
        return true;
    }
    return false;
}

page_id_t IxNodeHandle::internal_lookup(const char *key) {
    int pos = upper_bound(key);
    if (pos == 0) {
        return get_rid(0)->page_no;
    }
    return get_rid(pos - 1)->page_no;
}

void IxNodeHandle::insert_pairs(int pos, const char *key, const Rid *rid, int n) {
    assert(pos >= 0 && pos <= page_hdr->num_key);
    int num_key = page_hdr->num_key;
    for (int i = num_key - 1; i >= pos; i--) {
        memcpy(get_key(i + n), get_key(i), file_hdr->col_tot_len_);
        memcpy(get_rid(i + n), get_rid(i), sizeof(Rid));
    }
    for (int i = 0; i < n; i++) {
        memcpy(get_key(pos + i), key + i * file_hdr->col_tot_len_, file_hdr->col_tot_len_);
        *(get_rid(pos + i)) = rid[i];
    }
    page_hdr->num_key += n;
}

int IxNodeHandle::insert(const char *key, const Rid &value) {
    int pos = lower_bound(key);
    if (pos < page_hdr->num_key && ix_compare(get_key(pos), key, file_hdr->col_types_, file_hdr->col_lens_) == 0) {
        return page_hdr->num_key;
    }
    insert_pair(pos, key, value);
    return page_hdr->num_key;
}

void IxNodeHandle::erase_pair(int pos) {
    assert(pos >= 0 && pos < page_hdr->num_key);
    int num_key = page_hdr->num_key;
    for (int i = pos; i < num_key - 1; i++) {
        memcpy(get_key(i), get_key(i + 1), file_hdr->col_tot_len_);
        memcpy(get_rid(i), get_rid(i + 1), sizeof(Rid));
    }
    page_hdr->num_key--;
}

int IxNodeHandle::remove(const char *key) {
    int pos = lower_bound(key);
    if (pos < page_hdr->num_key && ix_compare(get_key(pos), key, file_hdr->col_types_, file_hdr->col_lens_) == 0) {
        erase_pair(pos);
        return page_hdr->num_key;
    }
    return -1;
}

IxIndexHandle::IxIndexHandle(DiskManager *disk_manager, BufferPoolManager *buffer_pool_manager, int fd)
    : disk_manager_(disk_manager), buffer_pool_manager_(buffer_pool_manager), fd_(fd) {
    disk_manager_->read_page(fd, IX_FILE_HDR_PAGE, (char *)&file_hdr_, sizeof(file_hdr_));
    char* buf = new char[PAGE_SIZE];
    memset(buf, 0, PAGE_SIZE);
    disk_manager_->read_page(fd, IX_FILE_HDR_PAGE, buf, PAGE_SIZE);
    file_hdr_ = new IxFileHdr();
    file_hdr_->deserialize(buf);

    int now_page_no = disk_manager_->get_fd2pageno(fd);
    disk_manager_->set_fd2pageno(fd, now_page_no + 1);
}

std::pair<IxNodeHandle *, bool> IxIndexHandle::find_leaf_page(const char *key, Operation operation,
                                                            Transaction *transaction, bool find_first) {
    page_id_t root_page_no = file_hdr_->root_page_;
    IxNodeHandle *node = fetch_node(root_page_no);

    while (!node->is_leaf_page()) {
        page_id_t child_page_no = node->internal_lookup(key);
        buffer_pool_manager_->unpin_page(node->get_page_id(), false);
        node = fetch_node(child_page_no);
    }

    return std::make_pair(node, false);
}

bool IxIndexHandle::get_value(const char *key, std::vector<Rid> *result, Transaction *transaction) {
    auto [leaf, root_latched] = find_leaf_page(key, Operation::FIND, transaction, false);
    Rid *rid;
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
    int left_size = total / 2;
    int right_size = total - left_size;

    new_node->insert_pairs(0, node->get_key(left_size), node->get_rid(left_size), right_size);
    node->set_size(left_size);

    if (node->is_leaf_page()) {
        new_node->set_prev_leaf(node->get_page_no());
        new_node->set_next_leaf(node->get_next_leaf());

        IxNodeHandle *next_leaf = fetch_node(node->get_next_leaf());
        next_leaf->set_prev_leaf(new_node->get_page_no());
        buffer_pool_manager_->unpin_page(next_leaf->get_page_id(), true);

        node->set_next_leaf(new_node->get_page_no());

        if (file_hdr_->last_leaf_ == node->get_page_no()) {
            file_hdr_->last_leaf_ = new_node->get_page_no();
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
        new_root->page_hdr->num_key = 0;

        Rid old_rid = {.page_no = old_node->get_page_no(), .slot_no = -1};
        new_root->insert_pair(0, old_node->get_key(0), old_rid);
        Rid new_rid = {.page_no = new_node->get_page_no(), .slot_no = -1};
        new_root->insert_pair(1, key, new_rid);

        old_node->set_parent_page_no(new_root->get_page_no());
        new_node->set_parent_page_no(new_root->get_page_no());
        file_hdr_->root_page_ = new_root->get_page_no();

        buffer_pool_manager_->unpin_page(new_root->get_page_id(), true);
        delete new_root;
    } else {
        IxNodeHandle *parent = fetch_node(old_node->get_parent_page_no());
        Rid new_rid = {.page_no = new_node->get_page_no(), .slot_no = -1};
        parent->insert_pair(parent->upper_bound(key), key, new_rid);

        if (parent->get_size() > file_hdr_->btree_order_) {
            IxNodeHandle *new_parent = split(parent);
            insert_into_parent(parent, new_parent->get_key(0), new_parent, transaction);
            buffer_pool_manager_->unpin_page(new_parent->get_page_id(), true);
            delete new_parent;
        }
        buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
        delete parent;
    }
}

page_id_t IxIndexHandle::insert_entry(const char *key, const Rid &value, Transaction *transaction) {
    auto [leaf, root_latched] = find_leaf_page(key, Operation::INSERT, transaction, false);

    int old_size = leaf->get_size();
    leaf->insert(key, value);

    page_id_t leaf_page_no = leaf->get_page_no();
    if (leaf->get_size() > file_hdr_->btree_order_) {
        IxNodeHandle *new_leaf = split(leaf);
        insert_into_parent(leaf, new_leaf->get_key(0), new_leaf, transaction);
        buffer_pool_manager_->unpin_page(new_leaf->get_page_id(), true);
        delete new_leaf;
    }

    buffer_pool_manager_->unpin_page(leaf->get_page_id(), true);
    delete leaf;
    return leaf_page_no;
}

bool IxIndexHandle::delete_entry(const char *key, Transaction *transaction) {
    auto [leaf, root_latched] = find_leaf_page(key, Operation::DELETE, transaction, false);

    int old_size = leaf->get_size();
    int new_size = leaf->remove(key);
    if (new_size == -1) {
        buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
        delete leaf;
        return false;
    }

    bool node_should_delete = coalesce_or_redistribute(leaf, transaction, nullptr);
    buffer_pool_manager_->unpin_page(leaf->get_page_id(), true);
    delete leaf;
    return true;
}

bool IxIndexHandle::coalesce_or_redistribute(IxNodeHandle *node, Transaction *transaction, bool *root_is_latched) {
    if (node->is_root_page()) {
        return adjust_root(node);
    }

    if (node->get_size() >= node->get_min_size()) {
        return false;
    }

    IxNodeHandle *parent = fetch_node(node->get_parent_page_no());
    int index = parent->find_child(node);

    if (index > 0) {
        IxNodeHandle *prev_node = fetch_node(parent->get_rid(index - 1)->page_no);
        if (prev_node->get_size() + node->get_size() >= 2 * node->get_min_size()) {
            redistribute(prev_node, node, parent, index);
            buffer_pool_manager_->unpin_page(prev_node->get_page_id(), true);
            buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
            return false;
        }
    }

    if (index < parent->get_size() - 1) {
        IxNodeHandle *next_node = fetch_node(parent->get_rid(index + 1)->page_no);
        if (next_node->get_size() + node->get_size() >= 2 * node->get_min_size()) {
            redistribute(node, next_node, parent, index + 1);
            buffer_pool_manager_->unpin_page(next_node->get_page_id(), true);
            buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
            return false;
        }
    }

    if (index > 0) {
        IxNodeHandle *prev_node = fetch_node(parent->get_rid(index - 1)->page_no);
        bool parent_should_delete = coalesce(&prev_node, &node, &parent, index, transaction, root_is_latched);
        buffer_pool_manager_->unpin_page(prev_node->get_page_id(), true);
        buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
        return parent_should_delete;
    } else {
        IxNodeHandle *next_node = fetch_node(parent->get_rid(1)->page_no);
        bool parent_should_delete = coalesce(&node, &next_node, &parent, 1, transaction, root_is_latched);
        buffer_pool_manager_->unpin_page(next_node->get_page_id(), true);
        buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
        return parent_should_delete;
    }
}

bool IxIndexHandle::adjust_root(IxNodeHandle *old_root_node) {
    if (!old_root_node->is_leaf_page() && old_root_node->get_size() == 1) {
        page_id_t child_page_no = old_root_node->get_rid(0)->page_no;
        IxNodeHandle *child = fetch_node(child_page_no);
        child->set_parent_page_no(IX_NO_PAGE);
        file_hdr_->root_page_ = child_page_no;
        buffer_pool_manager_->unpin_page(child->get_page_id(), true);
        release_node_handle(*old_root_node);
        return false;
    }

    if (old_root_node->is_leaf_page() && old_root_node->get_size() == 0) {
        file_hdr_->root_page_ = IX_NO_PAGE;
        release_node_handle(*old_root_node);
        return false;
    }

    return false;
}

void IxIndexHandle::redistribute(IxNodeHandle *neighbor_node, IxNodeHandle *node, IxNodeHandle *parent, int index) {
    if (index == 0) {
        char *first_key = neighbor_node->get_key(0);
        Rid first_rid = *neighbor_node->get_rid(0);
        node->insert_pair(node->get_size(), first_key, first_rid);
        neighbor_node->erase_pair(0);

        if (!neighbor_node->is_leaf_page()) {
            maintain_child(node, node->get_size() - 1);
        }

        memcpy(parent->get_key(1), neighbor_node->get_key(0), file_hdr_->col_tot_len_);
        maintain_parent(node);
    } else {
        int last_idx = neighbor_node->get_size() - 1;
        char *last_key = neighbor_node->get_key(last_idx);
        Rid last_rid = *neighbor_node->get_rid(last_idx);
        node->insert_pair(0, last_key, last_rid);
        neighbor_node->erase_pair(last_idx);

        if (!node->is_leaf_page()) {
            maintain_child(node, 0);
        }

        memcpy(parent->get_key(index), node->get_key(0), file_hdr_->col_tot_len_);
        maintain_parent(node);
    }
}

bool IxIndexHandle::coalesce(IxNodeHandle **neighbor_node, IxNodeHandle **node, IxNodeHandle **parent, int index,
                             Transaction *transaction, bool *root_is_latched) {
    IxNodeHandle *left_node = *neighbor_node;
    IxNodeHandle *right_node = *node;

    if (index == 0) {
        left_node = *node;
        right_node = *neighbor_node;
    }

    int left_size = left_node->get_size();
    int right_size = right_node->get_size();

    left_node->insert_pairs(left_size, right_node->get_key(0), right_node->get_rid(0), right_size);

    if (!left_node->is_leaf_page()) {
        for (int i = 0; i < right_size; i++) {
            maintain_child(left_node, left_size + i);
        }
    } else {
        left_node->set_next_leaf(right_node->get_next_leaf());
        IxNodeHandle *next_leaf = fetch_node(right_node->get_next_leaf());
        next_leaf->set_prev_leaf(left_node->get_page_no());
        buffer_pool_manager_->unpin_page(next_leaf->get_page_id(), true);

        if (file_hdr_->last_leaf_ == right_node->get_page_no()) {
            file_hdr_->last_leaf_ = left_node->get_page_no();
        }
    }

    release_node_handle(*right_node);

    int parent_child_idx = (*parent)->find_child(right_node);
    (*parent)->erase_pair(parent_child_idx);

    if ((*parent)->is_root_page()) {
        return adjust_root(*parent);
    }

    if ((*parent)->get_size() < (*parent)->get_min_size()) {
        return coalesce_or_redistribute(*parent, transaction, root_is_latched);
    }

    return false;
}

Rid IxIndexHandle::get_rid(const Iid &iid) const {
    IxNodeHandle *node = fetch_node(iid.page_no);
    if (iid.slot_no >= node->get_size()) {
        throw IndexEntryNotFoundError();
    }
    Rid result = *node->get_rid(iid.slot_no);
    buffer_pool_manager_->unpin_page(node->get_page_id(), false);
    delete node;
    return result;
}

Iid IxIndexHandle::lower_bound(const char *key) {
    auto [leaf, root_latched] = find_leaf_page(key, Operation::FIND, nullptr, false);
    int pos = leaf->lower_bound(key);
    Iid iid = {.page_no = leaf->get_page_no(), .slot_no = pos};
    buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
    delete leaf;
    return iid;
}

Iid IxIndexHandle::upper_bound(const char *key) {
    auto [leaf, root_latched] = find_leaf_page(key, Operation::FIND, nullptr, false);
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
            assert(buffer_pool_manager_->unpin_page(parent->get_page_id(), true));
            break;
        }
        memcpy(parent_key, child_first_key, file_hdr_->col_tot_len_);
        curr = parent;

        assert(buffer_pool_manager_->unpin_page(parent->get_page_id(), true));
    }
}

void IxIndexHandle::erase_leaf(IxNodeHandle *leaf) {
    assert(leaf->is_leaf_page());

    IxNodeHandle *prev = fetch_node(leaf->get_prev_leaf());
    prev->set_next_leaf(leaf->get_next_leaf());
    buffer_pool_manager_->unpin_page(prev->get_page_id(), true);

    IxNodeHandle *next = fetch_node(leaf->get_next_leaf());
    next->set_prev_leaf(leaf->get_prev_leaf());
    buffer_pool_manager_->unpin_page(next->get_page_id(), true);
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
    }
}