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

/**
 * @brief 在当前node中查找第一个>=target的key_idx
 *
 * @return key_idx，范围为[0,num_key)，如果返回的key_idx=num_key，则表示target大于最后一个key
 * @note 返回key index（同时也是rid index），作为slot no
 */
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

/**
 * @brief 在当前node中查找第一个>target的key_idx
 *
 * @return key_idx，范围为[1,num_key)，如果返回的key_idx=num_key，则表示target大于等于最后一个key
 * @note 注意此处的范围从1开始
 */
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

/**
 * @brief 用于叶子结点根据key来查找该结点中的键值对
 * 值value作为传出参数，函数返回是否查找成功
 *
 * @param key 目标key
 * @param[out] value 传出参数，目标key对应的Rid
 * @return 目标key是否存在
 */
bool IxNodeHandle::leaf_lookup(const char *key, Rid **value) {
    int pos = lower_bound(key);
    if (pos < page_hdr->num_key &&
        ix_compare(get_key(pos), key, file_hdr->col_types_, file_hdr->col_lens_) == 0) {
        *value = get_rid(pos);
        return true;
    }
    return false;
}

/**
 * 用于内部结点（非叶子节点）查找目标key所在的孩子结点（子树）
 * @param key 目标key
 * @return page_id_t 目标key所在的孩子节点（子树）的存储页面编号
 */
page_id_t IxNodeHandle::internal_lookup(const char *key) {
    // 使用upper_bound找到第一个大于key的位置，然后取其前一个孩子
    // 内部节点中 keys[i] 存储的是子树rids[i]中最小key值
    int pos = upper_bound(key);
    return value_at(pos - 1);
}

/**
 * @brief 在指定位置插入n个连续的键值对
 * 将key的前n位插入到原来keys中的pos位置；将rid的前n位插入到原来rids中的pos位置
 *
 * @param pos 要插入键值对的位置
 * @param (key, rid) 连续键值对的起始地址，也就是第一个键值对，可以通过(key, rid)来获取n个键值对
 * @param n 键值对数量
 */
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

/**
 * @brief 用于在结点中插入单个键值对。
 * 函数返回插入后的键值对数量
 *
 * @param (key, value) 要插入的键值对
 * @return int 键值对数量
 */
int IxNodeHandle::insert(const char *key, const Rid &value) {
    int pos = lower_bound(key);
    if (pos < page_hdr->num_key &&
        ix_compare(get_key(pos), key, file_hdr->col_types_, file_hdr->col_lens_) == 0) {
        return page_hdr->num_key;  // 唯一索引，重复key不插入
    }
    insert_pair(pos, key, value);
    return page_hdr->num_key;
}

/**
 * @brief 用于在结点中的指定位置删除单个键值对
 *
 * @param pos 要删除键值对的位置
 */
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

/**
 * @brief 用于在结点中删除指定key的键值对。函数返回删除后的键值对数量
 *
 * @param key 要删除的键值对key值
 * @return 完成删除操作后的键值对数量
 */
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
    // init file_hdr_
    disk_manager_->read_page(fd, IX_FILE_HDR_PAGE, (char *)&file_hdr_, sizeof(file_hdr_));
    char* buf = new char[PAGE_SIZE];
    memset(buf, 0, PAGE_SIZE);
    disk_manager_->read_page(fd, IX_FILE_HDR_PAGE, buf, PAGE_SIZE);
    file_hdr_ = new IxFileHdr();
    file_hdr_->deserialize(buf);
    
    // disk_manager管理的fd对应的文件中，设置从file_hdr_->num_pages开始分配page_no
    int now_page_no = disk_manager_->get_fd2pageno(fd);
    disk_manager_->set_fd2pageno(fd, now_page_no + 1);
}

/**
 * @brief 用于查找指定键所在的叶子结点
 */
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
        node = child;
    }
    return std::make_pair(node, false);
}

/**
 * @brief 用于查找指定键在叶子结点中的对应的值result
 */
bool IxIndexHandle::get_value(const char *key, std::vector<Rid> *result, Transaction *transaction) {
    auto [leaf, root_is_latched] = find_leaf_page(key, Operation::FIND, transaction, false);
    if (leaf == nullptr) return false;
    Rid *rid = nullptr;
    bool found = leaf->leaf_lookup(key, &rid);
    if (found) {
        result->push_back(*rid);
    }
    buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
    return found;
}

/**
 * @brief  将传入的一个node拆分(Split)成两个结点，在node的右边生成一个新结点new node
 */
IxNodeHandle *IxIndexHandle::split(IxNodeHandle *node) {
    IxNodeHandle *new_node = create_node();
    new_node->page_hdr->is_leaf = node->page_hdr->is_leaf;
    new_node->page_hdr->parent = node->page_hdr->parent;

    int total = node->get_size();
    int mid = total / 2;
    int move_cnt = total - mid;

    // 将右半部分移动到新节点
    new_node->insert_pairs(0, node->get_key(mid), node->get_rid(mid), move_cnt);
    node->set_size(mid);

    if (node->is_leaf_page()) {
        // 更新叶子节点链表
        new_node->set_prev_leaf(node->get_page_no());
        new_node->set_next_leaf(node->get_next_leaf());
        node->set_next_leaf(new_node->get_page_no());

        if (new_node->get_next_leaf() != IX_LEAF_HEADER_PAGE) {
            IxNodeHandle *next = fetch_node(new_node->get_next_leaf());
            next->set_prev_leaf(new_node->get_page_no());
            buffer_pool_manager_->unpin_page(next->get_page_id(), true);
        }

        if (file_hdr_->last_leaf_ == node->get_page_no()) {
            file_hdr_->last_leaf_ = new_node->get_page_no();
        }
    } else {
        // 更新孩子的父节点指针
        for (int i = 0; i < new_node->get_size(); i++) {
            maintain_child(new_node, i);
        }
    }

    return new_node;
}

/**
 * @brief Insert key & value pair into internal page after split
 */
void IxIndexHandle::insert_into_parent(IxNodeHandle *old_node, const char *key, IxNodeHandle *new_node,
                                     Transaction *transaction) {
    if (old_node->is_root_page()) {
        // 创建新的根节点
        IxNodeHandle *new_root = create_node();
        new_root->page_hdr->is_leaf = false;
        new_root->page_hdr->parent = IX_NO_PAGE;

        Rid old_rid = {.page_no = old_node->get_page_no(), .slot_no = -1};
        Rid new_rid = {.page_no = new_node->get_page_no(), .slot_no = -1};

        new_root->insert_pair(0, old_node->get_key(0), old_rid);
        new_root->insert_pair(1, new_node->get_key(0), new_rid);

        old_node->set_parent_page_no(new_root->get_page_no());
        new_node->set_parent_page_no(new_root->get_page_no());

        update_root_page_no(new_root->get_page_no());

        buffer_pool_manager_->unpin_page(new_root->get_page_id(), true);
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
        }

        buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
    }
}

/**
 * @brief 将指定键值对插入到B+树中
 */
page_id_t IxIndexHandle::insert_entry(const char *key, const Rid &value, Transaction *transaction) {
    if (is_empty()) {
        IxNodeHandle *root = fetch_node(file_hdr_->root_page_);
        root->insert(key, value);
        buffer_pool_manager_->unpin_page(root->get_page_id(), true);
        return root->get_page_no();
    }

    auto [leaf, root_latched] = find_leaf_page(key, Operation::INSERT, transaction, false);

    int old_size = leaf->get_size();
    leaf->insert(key, value);

    int leaf_page_no = leaf->get_page_no();

    if (leaf->get_size() == old_size) {
        // 重复key，未插入
        buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
        return leaf_page_no;
    }

    if (leaf->get_size() >= leaf->get_max_size()) {
        IxNodeHandle *new_leaf = split(leaf);
        const char *new_key = new_leaf->get_key(0);
        insert_into_parent(leaf, new_key, new_leaf, transaction);
        buffer_pool_manager_->unpin_page(new_leaf->get_page_id(), true);
    }

    // 更新last_leaf
    if (file_hdr_->last_leaf_ == IX_LEAF_HEADER_PAGE || leaf_page_no > file_hdr_->last_leaf_) {
        // scan through to find the actual last leaf
        IxNodeHandle *node = fetch_node(file_hdr_->first_leaf_);
        while (node->get_next_leaf() != IX_LEAF_HEADER_PAGE) {
            page_id_t next = node->get_next_leaf();
            buffer_pool_manager_->unpin_page(node->get_page_id(), false);
            node = fetch_node(next);
        }
        file_hdr_->last_leaf_ = node->get_page_no();
        buffer_pool_manager_->unpin_page(node->get_page_id(), false);
    }

    buffer_pool_manager_->unpin_page(leaf->get_page_id(), true);
    return leaf_page_no;
}

/**
 * @brief 用于删除B+树中含有指定key的键值对
 */
bool IxIndexHandle::delete_entry(const char *key, Transaction *transaction) {
    auto [leaf, root_latched] = find_leaf_page(key, Operation::DELETE, transaction, false);
    if (leaf == nullptr) return false;

    int old_size = leaf->get_size();
    leaf->remove(key);

    if (old_size == leaf->get_size()) {
        // Key未找到
        buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
        return false;
    }

    bool root_is_latched_out = false;
    coalesce_or_redistribute(leaf, transaction, &root_is_latched_out);

    buffer_pool_manager_->unpin_page(leaf->get_page_id(), true);
    return true;
}

/**
 * @brief 用于处理合并和重分配的逻辑，用于删除键值对后调用
 */
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

    // 优先使用前驱兄弟节点
    IxNodeHandle *neighbor = nullptr;
    if (rank > 0) {
        neighbor = fetch_node(parent->value_at(rank - 1));
    } else {
        neighbor = fetch_node(parent->value_at(rank + 1));
    }

    if (node->get_size() + neighbor->get_size() >= node->get_min_size() * 2) {
        redistribute(neighbor, node, parent, rank);
        buffer_pool_manager_->unpin_page(neighbor->get_page_id(), true);
        buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
        return false;
    } else {
        return coalesce(&neighbor, &node, &parent, rank, transaction, root_is_latched);
    }
}

/**
 * @brief 用于当根结点被删除了一个键值对之后的处理
 */
bool IxIndexHandle::adjust_root(IxNodeHandle *old_root_node) {
    if (old_root_node->is_leaf_page()) {
        if (old_root_node->get_size() == 0) {
            update_root_page_no(IX_NO_PAGE);
            file_hdr_->first_leaf_ = IX_NO_PAGE;
            file_hdr_->last_leaf_ = IX_NO_PAGE;
            release_node_handle(*old_root_node);
            return false;
        }
        return false;
    }

    // 内部节点
    if (old_root_node->get_size() == 1) {
        page_id_t child_page = old_root_node->value_at(0);
        IxNodeHandle *child = fetch_node(child_page);
        child->set_parent_page_no(IX_NO_PAGE);
        update_root_page_no(child_page);
        release_node_handle(*old_root_node);
        buffer_pool_manager_->unpin_page(child->get_page_id(), true);
        return true;  // old root needs to be deleted
    }

    return false;
}

/**
 * @brief 重新分配node和兄弟结点neighbor_node的键值对
 */
void IxIndexHandle::redistribute(IxNodeHandle *neighbor_node, IxNodeHandle *node, IxNodeHandle *parent, int index) {
    if (index > 0) {
        // neighbor是前驱节点（左兄弟）
        int neighbor_last = neighbor_node->get_size() - 1;
        node->insert_pair(0, neighbor_node->get_key(neighbor_last), *neighbor_node->get_rid(neighbor_last));
        neighbor_node->erase_pair(neighbor_last);

        // 更新父节点中node对应的key
        parent->set_key(index, node->get_key(0));

        if (!node->is_leaf_page()) {
            maintain_child(node, 0);
        }
    } else {
        // neighbor是后继节点（右兄弟），index == 0
        node->insert_pair(node->get_size(), neighbor_node->get_key(0), *neighbor_node->get_rid(0));
        neighbor_node->erase_pair(0);

        // 更新父节点中neighbor对应的key
        if (neighbor_node->get_size() > 0) {
            parent->set_key(1, neighbor_node->get_key(0));
        }

        if (!node->is_leaf_page()) {
            maintain_child(node, node->get_size() - 1);
        }
    }
}

/**
 * @brief 合并(Coalesce)函数
 */
bool IxIndexHandle::coalesce(IxNodeHandle **neighbor_node, IxNodeHandle **node, IxNodeHandle **parent, int index,
                             Transaction *transaction, bool *root_is_latched) {
    // 确保neighbor是左节点，node是右节点
    if (index == 0) {
        std::swap(*neighbor_node, *node);
        index = 1;
    }

    IxNodeHandle *left = *neighbor_node;
    IxNodeHandle *right = *node;
    IxNodeHandle *par = *parent;

    // 将右节点的所有键值对移动到左节点
    int left_size = left->get_size();
    left->insert_pairs(left_size, right->get_key(0), right->get_rid(0), right->get_size());

    if (right->is_leaf_page()) {
        // 更新叶子链表
        left->set_next_leaf(right->get_next_leaf());
        if (right->get_next_leaf() != IX_LEAF_HEADER_PAGE) {
            IxNodeHandle *next = fetch_node(right->get_next_leaf());
            next->set_prev_leaf(left->get_page_no());
            buffer_pool_manager_->unpin_page(next->get_page_id(), true);
        }
        if (file_hdr_->last_leaf_ == right->get_page_no()) {
            file_hdr_->last_leaf_ = left->get_page_no();
        }
    } else {
        // 更新孩子的父节点指针
        for (int i = left_size; i < left->get_size(); i++) {
            maintain_child(left, i);
        }
    }

    // 从父节点删除右节点对应的条目
    par->erase_pair(index);
    release_node_handle(*right);

    // 递归处理父节点
    if (par->is_root_page()) {
        return adjust_root(par);
    }

    if (par->get_size() < par->get_min_size()) {
        return coalesce_or_redistribute(par, transaction, root_is_latched);
    }

    return false;
}

/**
 * @brief 这里把iid转换成了rid
 */
Rid IxIndexHandle::get_rid(const Iid &iid) const {
    IxNodeHandle *node = fetch_node(iid.page_no);
    if (iid.slot_no >= node->get_size()) {
        throw IndexEntryNotFoundError();
    }
    buffer_pool_manager_->unpin_page(node->get_page_id(), false);  // unpin it!
    return *node->get_rid(iid.slot_no);
}

/**
 * @brief FindLeafPage + lower_bound
 */
Iid IxIndexHandle::lower_bound(const char *key) {
    auto [leaf, _] = find_leaf_page(key, Operation::FIND, nullptr, false);
    if (leaf == nullptr) return leaf_end();
    int pos = leaf->lower_bound(key);
    Iid iid = {.page_no = leaf->get_page_no(), .slot_no = pos};
    buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
    return iid;
}

/**
 * @brief FindLeafPage + upper_bound
 */
Iid IxIndexHandle::upper_bound(const char *key) {
    auto [leaf, _] = find_leaf_page(key, Operation::FIND, nullptr, false);
    if (leaf == nullptr) return leaf_end();
    int pos = leaf->upper_bound(key);
    Iid iid = {.page_no = leaf->get_page_no(), .slot_no = pos};
    buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
    return iid;
}

/**
 * @brief 指向最后一个叶子的最后一个结点的后一个
 */
Iid IxIndexHandle::leaf_end() const {
    IxNodeHandle *node = fetch_node(file_hdr_->last_leaf_);
    Iid iid = {.page_no = file_hdr_->last_leaf_, .slot_no = node->get_size()};
    buffer_pool_manager_->unpin_page(node->get_page_id(), false);  // unpin it!
    return iid;
}

/**
 * @brief 指向第一个叶子的第一个结点
 */
Iid IxIndexHandle::leaf_begin() const {
    Iid iid = {.page_no = file_hdr_->first_leaf_, .slot_no = 0};
    return iid;
}

/**
 * @brief 获取一个指定结点
 */
IxNodeHandle *IxIndexHandle::fetch_node(int page_no) const {
    Page *page = buffer_pool_manager_->fetch_page(PageId{fd_, page_no});
    IxNodeHandle *node = new IxNodeHandle(file_hdr_, page);
    
    return node;
}

/**
 * @brief 创建一个新结点
 */
IxNodeHandle *IxIndexHandle::create_node() {
    IxNodeHandle *node;
    file_hdr_->num_pages_++;

    PageId new_page_id = {.fd = fd_, .page_no = INVALID_PAGE_ID};
    Page *page = buffer_pool_manager_->new_page(&new_page_id);
    node = new IxNodeHandle(file_hdr_, page);
    return node;
}

/**
 * @brief 从node开始更新其父节点的第一个key，一直向上更新直到根节点
 */
void IxIndexHandle::maintain_parent(IxNodeHandle *node) {
    IxNodeHandle *curr = node;
    while (curr->get_parent_page_no() != IX_NO_PAGE) {
        // Load its parent
        IxNodeHandle *parent = fetch_node(curr->get_parent_page_no());
        int rank = parent->find_child(curr);
        char *parent_key = parent->get_key(rank);
        char *child_first_key = curr->get_key(0);
        if (memcmp(parent_key, child_first_key, file_hdr_->col_tot_len_) == 0) {
            assert(buffer_pool_manager_->unpin_page(parent->get_page_id(), true));
            break;
        }
        memcpy(parent_key, child_first_key, file_hdr_->col_tot_len_);  // 修改了parent node
        curr = parent;

        assert(buffer_pool_manager_->unpin_page(parent->get_page_id(), true));
    }
}

/**
 * @brief 要删除leaf之前调用此函数，更新leaf前驱结点的next指针和后继结点的prev指针
 */
void IxIndexHandle::erase_leaf(IxNodeHandle *leaf) {
    assert(leaf->is_leaf_page());

    IxNodeHandle *prev = fetch_node(leaf->get_prev_leaf());
    prev->set_next_leaf(leaf->get_next_leaf());
    buffer_pool_manager_->unpin_page(prev->get_page_id(), true);

    IxNodeHandle *next = fetch_node(leaf->get_next_leaf());
    next->set_prev_leaf(leaf->get_prev_leaf());  // 注意此处是SetPrevLeaf()
    buffer_pool_manager_->unpin_page(next->get_page_id(), true);
}

/**
 * @brief 删除node时，更新file_hdr_.num_pages
 */
void IxIndexHandle::release_node_handle(IxNodeHandle &node) {
    file_hdr_->num_pages_--;
}

/**
 * @brief 将node的第child_idx个孩子结点的父节点置为node
 */
void IxIndexHandle::maintain_child(IxNodeHandle *node, int child_idx) {
    if (!node->is_leaf_page()) {
        //  Current node is inner node, load its child and set its parent to current node
        int child_page_no = node->value_at(child_idx);
        IxNodeHandle *child = fetch_node(child_page_no);
        child->set_parent_page_no(node->get_page_no());
        buffer_pool_manager_->unpin_page(child->get_page_id(), true);
    }
}
