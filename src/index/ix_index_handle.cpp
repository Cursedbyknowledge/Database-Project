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

/**
 * @brief 在当前node中查找第一个>=target的key_idx
 *
 * @return key_idx，范围为[0,num_key)，如果返回的key_idx=num_key，则表示target大于最后一个key
 * @note 返回key index（同时也是rid index），作为slot no
 */
int IxNodeHandle::lower_bound(const char *target) const {
    int lo = 0, hi = page_hdr->num_key;
    if (binary_search) {
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (ix_compare(get_key(mid), target, file_hdr->col_types_, file_hdr->col_lens_) < 0) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
    } else {
        while (lo < hi) {
            int mid = lo;
            if (ix_compare(get_key(mid), target, file_hdr->col_types_, file_hdr->col_lens_) < 0) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
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
    int lo = 0, hi = page_hdr->num_key;
    if (binary_search) {
        while (lo < hi) {
            int mid = (lo + hi) / 2;
            if (ix_compare(get_key(mid), target, file_hdr->col_types_, file_hdr->col_lens_) <= 0) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
        }
    } else {
        while (lo < hi) {
            int mid = lo;
            if (ix_compare(get_key(mid), target, file_hdr->col_types_, file_hdr->col_lens_) <= 0) {
                lo = mid + 1;
            } else {
                hi = mid;
            }
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
 *
 * 内部节点结构：keys[0..num_key-1] 对应 rids[0..num_key-1]
 * key[i] 是子树 rids[i] 中的最小 key
 * 查找策略：找到最大的 i 使得 key[i] <= target，去 rids[i]；如果 target < key[0]，去 rids[0]
 */
page_id_t IxNodeHandle::internal_lookup(const char *key) {
    int pos = upper_bound(key);
    // pos 是第一个 > key 的位置，所以 target 可能在 rids[pos-1]
    if (pos > 0) {
        return get_rid(pos - 1)->page_no;
    }
    // target < key[0]，去第一个孩子
    return get_rid(0)->page_no;
}

/**
 * @brief 在指定位置插入n个连续的键值对
 * 将key的前n位插入到原来keys中的pos位置；将rid的前n位插入到原来rids中的pos位置
 *
 * @param pos 要插入键值对的位置
 * @param (key, rid) 连续键值对的起始地址，也就是第一个键值对，可以通过(key, rid)来获取n个键值对
 * @param n 键值对数量
 * @note [0,pos)           [pos,num_key)
 *                            key_slot
 *                            /      \
 *                           /        \
 *       [0,pos)     [pos,pos+n)   [pos+n,num_key+n)
 *                      key           key_slot
 */
void IxNodeHandle::insert_pairs(int pos, const char *key, const Rid *rid, int n) {
    assert(pos >= 0 && pos <= page_hdr->num_key);
    assert(page_hdr->num_key + n <= get_max_size());

    int num_key = page_hdr->num_key;
    int col_tot_len = file_hdr->col_tot_len_;

    // 将 [pos, num_key) 的 keys 向后移动 n 个位置
    int move_keys = (num_key - pos) * col_tot_len;
    if (move_keys > 0) {
        memmove(keys + (pos + n) * col_tot_len, keys + pos * col_tot_len, move_keys);
    }
    // 将 [pos, num_key) 的 rids 向后移动 n 个位置
    int move_rids = (num_key - pos) * sizeof(Rid);
    if (move_rids > 0) {
        memmove(rids + pos + n, rids + pos, move_rids);
    }
    // 插入 n 个新的 key
    memcpy(keys + pos * col_tot_len, key, n * col_tot_len);
    // 插入 n 个新的 rid
    memcpy(rids + pos, rid, n * sizeof(Rid));

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
    if (is_leaf_page() && pos < page_hdr->num_key &&
        ix_compare(get_key(pos), key, file_hdr->col_types_, file_hdr->col_lens_) == 0) {
        throw DuplicateKeyError();
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
    int num_key = page_hdr->num_key;
    int col_tot_len = file_hdr->col_tot_len_;

    // 将 [pos+1, num_key) 的 keys 向前移动
    int move_keys = (num_key - pos - 1) * col_tot_len;
    if (move_keys > 0) {
        memmove(keys + pos * col_tot_len, keys + (pos + 1) * col_tot_len, move_keys);
    }
    // 将 [pos+1, num_key) 的 rids 向前移动
    int move_rids = (num_key - pos - 1) * sizeof(Rid);
    if (move_rids > 0) {
        memmove(rids + pos, rids + pos + 1, move_rids);
    }
    // 清理最后一个位置（可选，安全起见）
    memset(keys + (num_key - 1) * col_tot_len, 0, col_tot_len);
    memset(&rids[num_key - 1], 0, sizeof(Rid));

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
    delete[] buf;
    
    // disk_manager管理的fd对应的文件中，设置从file_hdr_->num_pages开始分配page_no
    int now_page_no = disk_manager_->get_fd2pageno(fd);
    disk_manager_->set_fd2pageno(fd, now_page_no + 1);
}

/**
 * @brief 用于查找指定键所在的叶子结点
 * @param key 要查找的目标key值
 * @param operation 查找到目标键值对后要进行的操作类型
 * @param transaction 事务参数，如果不需要则默认传入nullptr
 * @return [leaf node] and [root_is_latched]
 * @note need to Unlatch and unpin the leaf node outside!
 * 注意：用了FindLeafPage之后一定要unlatch叶结点，否则下次latch该结点会堵塞！
 */
std::pair<IxNodeHandle *, bool> IxIndexHandle::find_leaf_page(const char *key, Operation operation,
                                                            Transaction *transaction, bool find_first) {
    // 如果树为空，返回nullptr
    if (is_empty()) {
        return std::make_pair(nullptr, false);
    }

    page_id_t root_page_no = file_hdr_->root_page_;
    page_id_t curr_page_no = root_page_no;

    IxNodeHandle *curr = fetch_node(curr_page_no);

    // 从根节点开始不断向下查找
    while (!curr->is_leaf_page()) {
        page_id_t child_page_no = curr->internal_lookup(key);
        IxNodeHandle *child = fetch_node(child_page_no);
        buffer_pool_manager_->unpin_page(curr->get_page_id(), false);
        curr = child;
    }

    return std::make_pair(curr, false);
}

/**
 * @brief 用于查找指定键在叶子结点中的对应的值result
 *
 * @param key 查找的目标key值
 * @param result 用于存放结果的容器
 * @param transaction 事务指针
 * @return bool 返回目标键值对是否存在
 */
bool IxIndexHandle::get_value(const char *key, std::vector<Rid> *result, Transaction *transaction) {
    auto [leaf, root_latched] = find_leaf_page(key, Operation::FIND, transaction, false);
    if (leaf == nullptr) {
        return false;
    }

    Rid *value = nullptr;
    bool found = leaf->leaf_lookup(key, &value);
    if (found && value != nullptr) {
        result->push_back(*value);
    }

    buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
    return found;
}

/**
 * @brief  将传入的一个node拆分(Split)成两个结点，在node的右边生成一个新结点new node
 * @param node 需要拆分的结点
 * @return 拆分得到的new_node
 * @note need to unpin the new node outside
 */
IxNodeHandle *IxIndexHandle::split(IxNodeHandle *node) {
    IxNodeHandle *new_node = create_node();
    new_node->page_hdr->is_leaf = node->page_hdr->is_leaf;
    new_node->page_hdr->parent = node->page_hdr->parent;

    int old_size = node->get_size();
    // 将后半部分移到新节点
    int split_pos = old_size / 2;
    int move_n = old_size - split_pos;

    // 移动键值对到新节点
    new_node->insert_pairs(0, node->get_key(split_pos), node->get_rid(split_pos), move_n);

    // 更新旧节点大小
    node->set_size(split_pos);

    // 如果是叶子节点，更新链表指针
    if (node->is_leaf_page()) {
        new_node->set_prev_leaf(node->get_page_no());
        new_node->set_next_leaf(node->get_next_leaf());
        node->set_next_leaf(new_node->get_page_no());

        // 更新new_node后继节点的前驱指针
        if (new_node->get_next_leaf() != IX_LEAF_HEADER_PAGE) {
            IxNodeHandle *next = fetch_node(new_node->get_next_leaf());
            next->set_prev_leaf(new_node->get_page_no());
            buffer_pool_manager_->unpin_page(next->get_page_id(), true);
        }

        // 如果 node 是最后一个叶子，更新 last_leaf
        if (file_hdr_->last_leaf_ == node->get_page_no()) {
            file_hdr_->last_leaf_ = new_node->get_page_no();
        }
    } else {
        // 如果是内部节点，更新移动过去的孩子节点的父指针
        for (int i = 0; i < new_node->get_size(); i++) {
            maintain_child(new_node, i);
        }
    }

    return new_node;
}

/**
 * @brief Insert key & value pair into internal page after split
 * 拆分(Split)后，向上找到old_node的父结点
 * 将new_node的第一个key插入到父结点，其位置在 父结点指向old_node的孩子指针 之后
 * 如果插入后>=maxsize，则必须继续拆分父结点，然后在其父结点的父结点再插入，即需要递归
 * 直到找到的old_node为根结点时，结束递归（此时将会新建一个根R，关键字为key，old_node和new_node为其孩子）
 *
 * @param (old_node, new_node) 原结点为old_node，old_node被分裂之后产生了新的右兄弟结点new_node
 * @param key 要插入parent的key
 * @note 本函数执行完毕后，new node和old node都需要在函数外面进行unpin
 */
void IxIndexHandle::insert_into_parent(IxNodeHandle *old_node, const char *key, IxNodeHandle *new_node,
                                     Transaction *transaction) {
    // 如果 old_node 是根节点，创建新根
    if (old_node->is_root_page()) {
        IxNodeHandle *new_root = create_node();
        new_root->page_hdr->is_leaf = false;
        new_root->page_hdr->parent = IX_NO_PAGE;

        // 将 old_node 和 new_node 作为新根的两个孩子
        // old_node 的 key 作为第一个条目，new_node 的第一个 key 作为第二个条目
        Rid rid_old;
        rid_old.page_no = old_node->get_page_no();
        rid_old.slot_no = 0;
        Rid rid_new;
        rid_new.page_no = new_node->get_page_no();
        rid_new.slot_no = 0;

        // 先插入 old_node 的 key 和 rid
        new_root->insert_pair(0, old_node->get_key(0), rid_old);
        // 再插入 new_node 的 key 和 rid
        new_root->insert_pair(1, key, rid_new);

        // 更新 old_node 和 new_node 的父指针
        old_node->set_parent_page_no(new_root->get_page_no());
        new_node->set_parent_page_no(new_root->get_page_no());

        // 更新根的父指针
        new_root->set_parent_page_no(IX_NO_PAGE);

        // 更新 file_hdr 中的根页面
        update_root_page_no(new_root->get_page_no());

        buffer_pool_manager_->unpin_page(new_root->get_page_id(), true);
        return;
    }

    // 非根节点：找到父节点
    IxNodeHandle *parent = fetch_node(old_node->get_parent_page_no());

    // 找到 old_node 在 parent 中的位置
    int rank = parent->find_child(old_node);

    // 构建要插入的键值对：new_node 的第一个 key 和 new_node 的 rid
    Rid rid_new;
    rid_new.page_no = new_node->get_page_no();
    rid_new.slot_no = 0;

    parent->insert_pair(rank + 1, key, rid_new);

    new_node->set_parent_page_no(parent->get_page_no());

    // 如果 parent 满了，需要继续分裂
    if (parent->get_size() >= parent->get_max_size()) {
        IxNodeHandle *new_parent = split(parent);
        // 取 new_parent 的第一个 key
        insert_into_parent(parent, new_parent->get_key(0), new_parent, transaction);
        buffer_pool_manager_->unpin_page(new_parent->get_page_id(), true);
    }

    buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
}

/**
 * @brief 将指定键值对插入到B+树中
 * @param (key, value) 要插入的键值对
 * @param transaction 事务指针
 * @return page_id_t 插入到的叶结点的page_no
 */
page_id_t IxIndexHandle::insert_entry(const char *key, const Rid &value, Transaction *transaction) {
    if (is_empty()) {
        // 树为空，创建根节点
        IxNodeHandle *new_root = create_node();
        new_root->page_hdr->is_leaf = true;
        new_root->page_hdr->parent = IX_NO_PAGE;
        new_root->set_prev_leaf(IX_LEAF_HEADER_PAGE);
        new_root->set_next_leaf(IX_LEAF_HEADER_PAGE);

        // 更新叶子链表头指针
        IxNodeHandle *leaf_header = fetch_node(IX_LEAF_HEADER_PAGE);
        leaf_header->set_next_leaf(new_root->get_page_no());
        leaf_header->set_prev_leaf(new_root->get_page_no());
        buffer_pool_manager_->unpin_page(leaf_header->get_page_id(), true);

        // 插入键值对
        new_root->insert_pair(0, key, value);

        // 更新根页面
        update_root_page_no(new_root->get_page_no());
        file_hdr_->first_leaf_ = new_root->get_page_no();
        file_hdr_->last_leaf_ = new_root->get_page_no();

        page_id_t leaf_page = new_root->get_page_no();
        buffer_pool_manager_->unpin_page(new_root->get_page_id(), true);
        return leaf_page;
    }

    // 找到目标叶子节点
    auto [leaf, root_latched] = find_leaf_page(key, Operation::INSERT, transaction, false);

    // 检查唯一性：leaf_lookup
    Rid *existing = nullptr;
    if (leaf->leaf_lookup(key, &existing)) {
        // 已存在，唯一性约束违反
        buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
        throw DuplicateKeyError();
    }

    // 插入键值对
    leaf->insert(key, value);

    page_id_t leaf_page = leaf->get_page_no();

    // 如果叶子节点满了，需要分裂
    if (leaf->get_size() >= leaf->get_max_size()) {
        IxNodeHandle *new_leaf = split(leaf);
        // 将 new_leaf 的第一个 key 插入到父节点
        insert_into_parent(leaf, new_leaf->get_key(0), new_leaf, transaction);
        buffer_pool_manager_->unpin_page(new_leaf->get_page_id(), true);
    }

    buffer_pool_manager_->unpin_page(leaf->get_page_id(), true);
    return leaf_page;
}

/**
 * @brief 用于删除B+树中含有指定key的键值对
 * @param key 要删除的key值
 * @param transaction 事务指针
 */
bool IxIndexHandle::delete_entry(const char *key, Transaction *transaction) {
    if (is_empty()) {
        return false;
    }

    auto [leaf, root_latched] = find_leaf_page(key, Operation::DELETE, transaction, false);
    if (leaf == nullptr) {
        return false;
    }

    // 检查 key 是否存在
    int pos = leaf->lower_bound(key);
    if (pos >= leaf->get_size() ||
        ix_compare(leaf->get_key(pos), key, file_hdr_->col_types_, file_hdr_->col_lens_) != 0) {
        buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
        return false;
    }

    // 删除键值对
    leaf->erase_pair(pos);

    // 如果删除后需要合并或重分配
    bool root_latched_flag = root_latched;
    coalesce_or_redistribute(leaf, transaction, &root_latched_flag);

    buffer_pool_manager_->unpin_page(leaf->get_page_id(), true);
    return true;
}

/**
 * @brief 用于处理合并和重分配的逻辑，用于删除键值对后调用
 *
 * @param node 执行完删除操作的结点
 * @param transaction 事务指针
 * @param root_is_latched 传出参数：根节点是否上锁
 * @return 是否需要删除结点
 */
bool IxIndexHandle::coalesce_or_redistribute(IxNodeHandle *node, Transaction *transaction, bool *root_is_latched) {
    // 如果 node 是根节点
    if (node->is_root_page()) {
        return adjust_root(node);
    }

    // 如果节点大小满足最小值，不需要操作
    if (node->get_size() >= node->get_min_size()) {
        // 但是需要检查是否需要 maintain_parent（如果是叶子且删除了最小key）
        if (node->is_leaf_page() && node->get_size() > 0) {
            maintain_parent(node);
        }
        return false;
    }

    // 找到父节点
    IxNodeHandle *parent = fetch_node(node->get_parent_page_no());

    // 找到 node 在 parent 中的位置
    int index = parent->find_child(node);

    // 找到兄弟节点（优先前驱）
    int neighbor_index;
    IxNodeHandle *neighbor;
    if (index > 0) {
        // 前驱兄弟
        neighbor_index = index - 1;
        neighbor = fetch_node(parent->get_rid(neighbor_index)->page_no);
    } else {
        // 后继兄弟
        neighbor_index = index + 1;
        neighbor = fetch_node(parent->get_rid(neighbor_index)->page_no);
    }

    // 如果合并后可以满足最小值要求，则重分配
    if (node->get_size() + neighbor->get_size() >= node->get_min_size() * 2) {
        redistribute(neighbor, node, parent, index);
        buffer_pool_manager_->unpin_page(neighbor->get_page_id(), true);
        buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
        return false;
    }

    // 否则合并
    bool parent_should_delete = coalesce(&neighbor, &node, &parent, index, transaction, root_is_latched);

    buffer_pool_manager_->unpin_page(neighbor->get_page_id(), true);
    buffer_pool_manager_->unpin_page(node->get_page_id(), true);

    if (parent_should_delete) {
        bool dummy;
        coalesce_or_redistribute(parent, transaction, &dummy);
    } else {
        buffer_pool_manager_->unpin_page(parent->get_page_id(), true);
    }
    return false;
}

/**
 * @brief 用于当根结点被删除了一个键值对之后的处理
 * @param old_root_node 原根节点
 * @return bool 根结点是否需要被删除
 */
bool IxIndexHandle::adjust_root(IxNodeHandle *old_root_node) {
    // 如果根是内部节点且只有一个孩子，将其孩子提升为新根
    if (!old_root_node->is_leaf_page() && old_root_node->get_size() == 1) {
        page_id_t child_page_no = old_root_node->get_rid(0)->page_no;
        IxNodeHandle *child = fetch_node(child_page_no);
        child->set_parent_page_no(IX_NO_PAGE);
        update_root_page_no(child_page_no);
        buffer_pool_manager_->unpin_page(child->get_page_id(), true);

        // 释放旧根
        release_node_handle(*old_root_node);
        return true;
    }

    // 如果根是叶子且为空
    if (old_root_node->is_leaf_page() && old_root_node->get_size() == 0) {
        update_root_page_no(IX_NO_PAGE);
        file_hdr_->first_leaf_ = IX_LEAF_HEADER_PAGE;
        file_hdr_->last_leaf_ = IX_LEAF_HEADER_PAGE;
        release_node_handle(*old_root_node);
        return true;
    }

    return false;
}

/**
 * @brief 重新分配node和兄弟结点neighbor_node的键值对
 *
 * @param neighbor_node sibling page of input "node"
 * @param node input from method coalesceOrRedistribute()
 * @param parent the parent of "node" and "neighbor_node"
 * @param index node在parent中的rid_idx
 * @note node是之前刚被删除过一个key的结点
 * index=0，则neighbor是node后继结点，表示：node(left)      neighbor(right)
 * index>0，则neighbor是node前驱结点，表示：neighbor(left)  node(right)
 */
void IxIndexHandle::redistribute(IxNodeHandle *neighbor_node, IxNodeHandle *node, IxNodeHandle *parent, int index) {
    if (index == 0) {
        // node 在左边，neighbor 在右边；从 neighbor 移一个键值对到 node
        // 取 neighbor 的第一个键值对
        int move_pos = 0;
        node->insert_pair(node->get_size(), neighbor_node->get_key(move_pos), *neighbor_node->get_rid(move_pos));
        neighbor_node->erase_pair(move_pos);

        // 更新 parent 中 node 的 key
        if (node->is_leaf_page()) {
            maintain_parent(node);
        } else {
            maintain_child(node, node->get_size() - 1);
        }
        // 更新 parent 中的 separator key
        parent->set_key(index, neighbor_node->get_key(0));
    } else {
        // neighbor 在左边，node 在右边；从 neighbor 移最后一个键值对到 node
        int move_pos = neighbor_node->get_size() - 1;
        node->insert_pair(0, neighbor_node->get_key(move_pos), *neighbor_node->get_rid(move_pos));
        neighbor_node->erase_pair(move_pos);

        // 更新 parent 中 node 的 key
        if (node->is_leaf_page()) {
            maintain_parent(node);
        } else {
            maintain_child(node, 0);
        }
        // 更新 parent 中的 separator key
        parent->set_key(index, node->get_key(0));
    }
}

/**
 * @brief 合并(Coalesce)函数是将node和其直接前驱进行合并，也就是和它左边的neighbor_node进行合并；
 * 假设node一定在右边。如果上层传入的index=0，说明node在左边，那么交换node和neighbor_node，保证node在右边
 *
 * @param neighbor_node sibling page of input "node" (neighbor_node是node的前结点)
 * @param node input from method coalesceOrRedistribute() (node结点是需要被删除的)
 * @param parent parent page of input "node"
 * @param index node在parent中的rid_idx
 * @return true means parent node should be deleted, false means no deletion happend
 */
bool IxIndexHandle::coalesce(IxNodeHandle **neighbor_node, IxNodeHandle **node, IxNodeHandle **parent, int index,
                             Transaction *transaction, bool *root_is_latched) {
    // 确保 neighbor 在左边，node 在右边
    if (index == 0) {
        // node 在左边，交换
        std::swap(*neighbor_node, *node);
        index = 1;
    }

    int neighbor_size = (*neighbor_node)->get_size();
    int node_size = (*node)->get_size();
    int col_tot_len = file_hdr_->col_tot_len_;

    if ((*node)->is_leaf_page()) {
        // 叶子节点：把 node 的所有键值对移到 neighbor
        (*neighbor_node)->insert_pairs(neighbor_size, (*node)->get_key(0), (*node)->get_rid(0), node_size);

        // 更新叶子链表
        (*neighbor_node)->set_next_leaf((*node)->get_next_leaf());
        if ((*node)->get_next_leaf() != IX_LEAF_HEADER_PAGE) {
            IxNodeHandle *next = fetch_node((*node)->get_next_leaf());
            next->set_prev_leaf((*neighbor_node)->get_page_no());
            buffer_pool_manager_->unpin_page(next->get_page_id(), true);
        }
        // 更新 last_leaf
        if (file_hdr_->last_leaf_ == (*node)->get_page_no()) {
            file_hdr_->last_leaf_ = (*neighbor_node)->get_page_no();
        }
    } else {
        // 内部节点：先把 parent 中对应 node 的 key 移过来作为 separator
        // 把 parent 中 node 的 key 移入 neighbor
        (*neighbor_node)->insert_pair(neighbor_size, (*parent)->get_key(index - 1), *(*parent)->get_rid(index - 1));

        // 再把 node 的所有键值对移到 neighbor
        (*neighbor_node)->insert_pairs((*neighbor_node)->get_size(), (*node)->get_key(0), (*node)->get_rid(0), node_size);

        // 更新孩子节点的父指针
        for (int i = 0; i < (*neighbor_node)->get_size(); i++) {
            maintain_child(*neighbor_node, i);
        }
    }

    // 从 parent 中删除 node
    (*parent)->erase_pair(index - 1);

    // 释放 node
    release_node_handle(**node);

    // 如果 parent 太小，返回 true 让上层处理
    return (*parent)->get_size() < (*parent)->get_min_size();
}

/**
 * @brief 这里把iid转换成了rid，即iid的slot_no作为node的rid_idx(key_idx)
 * node其实就是把slot_no作为键值对数组的下标
 * 换而言之，每个iid对应的索引槽存了一对(key,rid)，指向了(要建立索引的属性首地址,插入/删除记录的位置)
 *
 * @param iid
 * @return Rid
 */
Rid IxIndexHandle::get_rid(const Iid &iid) const {
    IxNodeHandle *node = fetch_node(iid.page_no);
    if (iid.slot_no >= node->get_size()) {
        throw IndexEntryNotFoundError();
    }
    buffer_pool_manager_->unpin_page(node->get_page_id(), false);
    return *node->get_rid(iid.slot_no);
}

/**
 * @brief FindLeafPage + lower_bound
 *
 * @param key
 * @return Iid
 * @note 上层传入的key本来是int类型，通过(const char *)&key进行了转换
 */
Iid IxIndexHandle::lower_bound(const char *key) {
    if (is_empty()) {
        return leaf_end();
    }
    auto [leaf, root_latched] = find_leaf_page(key, Operation::FIND, nullptr, false);
    int pos = leaf->lower_bound(key);
    Iid iid = {.page_no = leaf->get_page_no(), .slot_no = pos};
    buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
    return iid;
}

/**
 * @brief FindLeafPage + upper_bound
 *
 * @param key
 * @return Iid
 */
Iid IxIndexHandle::upper_bound(const char *key) {
    if (is_empty()) {
        return leaf_end();
    }
    auto [leaf, root_latched] = find_leaf_page(key, Operation::FIND, nullptr, true);
    int pos = leaf->upper_bound(key);
    Iid iid = {.page_no = leaf->get_page_no(), .slot_no = pos};
    // 如果 upper_bound 在叶子节点末尾，可能需要跨到下一个叶子
    if (pos >= leaf->get_size() && leaf->get_next_leaf() != IX_LEAF_HEADER_PAGE) {
        iid.page_no = leaf->get_next_leaf();
        iid.slot_no = 0;
    }
    buffer_pool_manager_->unpin_page(leaf->get_page_id(), false);
    return iid;
}

/**
 * @brief 指向最后一个叶子的最后一个结点的后一个
 * 用处在于可以作为IxScan的最后一个
 *
 * @return Iid
 */
Iid IxIndexHandle::leaf_end() const {
    IxNodeHandle *node = fetch_node(file_hdr_->last_leaf_);
    Iid iid = {.page_no = file_hdr_->last_leaf_, .slot_no = node->get_size()};
    buffer_pool_manager_->unpin_page(node->get_page_id(), false);
    return iid;
}

/**
 * @brief 指向第一个叶子的第一个结点
 * 用处在于可以作为IxScan的第一个
 *
 * @return Iid
 */
Iid IxIndexHandle::leaf_begin() const {
    Iid iid = {.page_no = file_hdr_->first_leaf_, .slot_no = 0};
    return iid;
}

/**
 * @brief 获取一个指定结点
 *
 * @param page_no
 * @return IxNodeHandle*
 * @note pin the page, remember to unpin it outside!
 */
IxNodeHandle *IxIndexHandle::fetch_node(int page_no) const {
    Page *page = buffer_pool_manager_->fetch_page(PageId{fd_, page_no});
    IxNodeHandle *node = new IxNodeHandle(file_hdr_, page);
    
    return node;
}

/**
 * @brief 创建一个新结点
 *
 * @return IxNodeHandle*
 * @note pin the page, remember to unpin it outside!
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
 *
 * @param node
 */
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

/**
 * @brief 要删除leaf之前调用此函数，更新leaf前驱结点的next指针和后继结点的prev指针
 *
 * @param leaf 要删除的leaf
 */
void IxIndexHandle::erase_leaf(IxNodeHandle *leaf) {
    assert(leaf->is_leaf_page());

    IxNodeHandle *prev = fetch_node(leaf->get_prev_leaf());
    prev->set_next_leaf(leaf->get_next_leaf());
    buffer_pool_manager_->unpin_page(prev->get_page_id(), true);

    IxNodeHandle *next = fetch_node(leaf->get_next_leaf());
    next->set_prev_leaf(leaf->get_prev_leaf());
    buffer_pool_manager_->unpin_page(next->get_page_id(), true);
}

/**
 * @brief 删除node时，更新file_hdr_.num_pages
 *
 * @param node
 */
void IxIndexHandle::release_node_handle(IxNodeHandle &node) {
    file_hdr_->num_pages_--;
}

/**
 * @brief 将node的第child_idx个孩子结点的父节点置为node
 */
void IxIndexHandle::maintain_child(IxNodeHandle *node, int child_idx) {
    if (!node->is_leaf_page()) {
        int child_page_no = node->value_at(child_idx);
        IxNodeHandle *child = fetch_node(child_page_no);
        child->set_parent_page_no(node->get_page_no());
        buffer_pool_manager_->unpin_page(child->get_page_id(), true);
    }
}
