/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include "sm_manager.h"

#include <sys/stat.h>
#include <unistd.h>

#include <fstream>

#include "index/ix.h"
#include "record/rm.h"
#include "record_printer.h"

/**
 * @description: 判断是否为一个文件夹
 * @return {bool} 返回是否为一个文件夹
 * @param {string&} db_name 数据库文件名称，与文件夹同名
 */
bool SmManager::is_dir(const std::string& db_name) {
    struct stat st;
    return stat(db_name.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

/**
 * @description: 创建数据库，所有的数据库相关文件都放在数据库同名文件夹下
 * @param {string&} db_name 数据库名称
 */
void SmManager::create_db(const std::string& db_name) {
    if (is_dir(db_name)) {
        throw DatabaseExistsError(db_name);
    }
    //为数据库创建一个子目录
    std::string cmd = "mkdir " + db_name;
    if (system(cmd.c_str()) < 0) {  // 创建一个名为db_name的目录
        throw UnixError();
    }
    if (chdir(db_name.c_str()) < 0) {  // 进入名为db_name的目录
        throw UnixError();
    }
    //创建系统目录
    DbMeta *new_db = new DbMeta();
    new_db->name_ = db_name;

    // 注意，此处ofstream会在当前目录创建(如果没有此文件先创建)和打开一个名为DB_META_NAME的文件
    std::ofstream ofs(DB_META_NAME);

    // 将new_db中的信息，按照定义好的operator<<操作符，写入到ofs打开的DB_META_NAME文件中
    ofs << *new_db;  // 注意：此处重载了操作符<<

    delete new_db;

    // 创建日志文件
    disk_manager_->create_file(LOG_FILE_NAME);

    // 回到根目录
    if (chdir("..") < 0) {
        throw UnixError();
    }
}

/**
 * @description: 删除数据库，同时需要清空相关文件以及数据库同名文件夹
 * @param {string&} db_name 数据库名称，与文件夹同名
 */
void SmManager::drop_db(const std::string& db_name) {
    if (!is_dir(db_name)) {
        throw DatabaseNotFoundError(db_name);
    }
    std::string cmd = "rm -r " + db_name;
    if (system(cmd.c_str()) < 0) {
        throw UnixError();
    }
}

/**
 * @description: 打开数据库，找到数据库对应的文件夹，并加载数据库元数据和相关文件
 * @param {string&} db_name 数据库名称，与文件夹同名
 */
void SmManager::open_db(const std::string& db_name) {
    if (chdir(db_name.c_str()) < 0) {
        throw UnixError();
    }
    std::ifstream ifs(DB_META_NAME);
    ifs >> db_;
    for (auto &entry : db_.tabs_) {
        fhs_.emplace(entry.first, rm_manager_->open_file(entry.first));
    }
    // 索引文件通过 get_ih() 懒加载打开，不在此处预加载
    if (chdir("..") < 0) {
        throw UnixError();
    }
}

/**
 * @description: 把数据库相关的元数据刷入磁盘中
 */
void SmManager::flush_meta() {
    // 默认清空文件
    std::ofstream ofs(DB_META_NAME);
    ofs << db_;
}

/**
 * @description: 关闭数据库并把数据落盘
 */
void SmManager::close_db() {
    for (auto &entry : fhs_) {
        rm_manager_->close_file(entry.second.get());
    }
    fhs_.clear();
    flush_meta();
}

/**
 * @description: 显示所有的表,通过测试需要将其结果写入到output.txt,详情看题目文档
 * @param {Context*} context 
 */
void SmManager::show_tables(Context* context) {
    std::fstream outfile;
    outfile.open("output.txt", std::ios::out | std::ios::app);
    outfile << "| Tables |\n";
    RecordPrinter printer(1);
    printer.print_separator(context);
    printer.print_record({"Tables"}, context);
    printer.print_separator(context);
    for (auto &entry : db_.tabs_) {
        auto &tab = entry.second;
        printer.print_record({tab.name}, context);
        outfile << "| " << tab.name << " |\n";
    }
    printer.print_separator(context);
    outfile.close();
}

/**
 * @description: 显示表的元数据
 * @param {string&} tab_name 表名称
 * @param {Context*} context 
 */
void SmManager::desc_table(const std::string& tab_name, Context* context) {
    TabMeta &tab = db_.get_table(tab_name);

    std::vector<std::string> captions = {"Field", "Type", "Index"};
    RecordPrinter printer(captions.size());
    // Print header
    printer.print_separator(context);
    printer.print_record(captions, context);
    printer.print_separator(context);
    // Print fields
    for (auto &col : tab.cols) {
        std::vector<std::string> field_info = {col.name, coltype2str(col.type), col.index ? "YES" : "NO"};
        printer.print_record(field_info, context);
    }
    // Print footer
    printer.print_separator(context);
}

/**
 * @description: 创建表
 * @param {string&} tab_name 表的名称
 * @param {vector<ColDef>&} col_defs 表的字段
 * @param {Context*} context 
 */
void SmManager::create_table(const std::string& tab_name, const std::vector<ColDef>& col_defs, Context* context) {
    if (db_.is_table(tab_name)) {
        throw TableExistsError(tab_name);
    }
    // Create table meta
    int curr_offset = 0;
    TabMeta tab;
    tab.name = tab_name;
    for (auto &col_def : col_defs) {
        ColMeta col = {.tab_name = tab_name,
                       .name = col_def.name,
                       .type = col_def.type,
                       .len = col_def.len,
                       .offset = curr_offset,
                       .index = false};
        curr_offset += col_def.len;
        tab.cols.push_back(col);
    }
    // Create & open record file
    int record_size = curr_offset;  // record_size就是col meta所占的大小（表的元数据也是以记录的形式进行存储的）
    rm_manager_->create_file(tab_name, record_size);
    db_.tabs_[tab_name] = tab;
    // fhs_[tab_name] = rm_manager_->open_file(tab_name);
    fhs_.emplace(tab_name, rm_manager_->open_file(tab_name));

    flush_meta();
}

/**
 * @description: 删除表
 * @param {string&} tab_name 表的名称
 * @param {Context*} context
 */
void SmManager::drop_table(const std::string& tab_name, Context* context) {
    auto it = fhs_.find(tab_name);
    if (it == fhs_.end()) {
        throw TableNotFoundError(tab_name);
    }
    // 先删除表上的所有索引
    TabMeta &tab = db_.get_table(tab_name);
    while (!tab.indexes.empty()) {
        auto &index = tab.indexes.back();
        std::vector<std::string> col_names;
        for (auto &col : index.cols) {
            col_names.push_back(col.name);
        }
        drop_index(tab_name, col_names, context);
    }
    rm_manager_->close_file(it->second.get());
    fhs_.erase(it);
    rm_manager_->destroy_file(tab_name);
    db_.tabs_.erase(tab_name);
    flush_meta();
}

/**
 * @description: 创建索引
 * @param {string&} tab_name 表的名称
 * @param {vector<string>&} col_names 索引包含的字段名称
 * @param {Context*} context
 */
void SmManager::create_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context) {
    TabMeta &tab = db_.get_table(tab_name);

    // 检查索引是否已存在
    if (tab.is_index(col_names)) {
        throw IndexExistsError(tab_name, col_names);
    }

    // 验证所有字段存在且获取其元数据
    std::vector<ColMeta> index_cols;
    for (auto &col_name : col_names) {
        auto col_it = tab.get_col(col_name);
        index_cols.push_back(*col_it);
    }

    // 创建索引文件（物理文件）
    ix_manager_->create_index(tab_name, index_cols);

    // 记录索引元数据
    IndexMeta index_meta;
    index_meta.tab_name = tab_name;
    index_meta.col_num = col_names.size();
    index_meta.col_tot_len = 0;
    for (auto &col : index_cols) {
        index_meta.cols.push_back(col);
        index_meta.col_tot_len += col.len;
    }
    tab.indexes.push_back(index_meta);

    // 打开索引并构建B+树：扫描全表数据并插入索引
    auto ih = ix_manager_->open_index(tab_name, index_cols);
    auto fh = fhs_.at(tab_name).get();
    auto scan = std::make_unique<RmScan>(fh);

    for (scan->next(); !scan->is_end(); scan->next()) {
        auto rec = fh->get_record(scan->rid(), context);
        // 构造索引 key：按照 index_cols 的顺序拼接字段值
        char *key = new char[index_meta.col_tot_len];
        int offset = 0;
        for (auto &col : index_cols) {
            memcpy(key + offset, rec->data + col.offset, col.len);
            offset += col.len;
        }
        // 插入到 B+ 树中
        try {
            ih->insert_entry(key, scan->rid(), context->txn_);
        } catch (DuplicateKeyError &e) {
            // 表中已有重复值，回滚：删除索引文件
            delete[] key;
            ih.reset();
            ix_manager_->destroy_index(tab_name, index_cols);
            tab.indexes.pop_back();
            throw;
        }
        delete[] key;
    }

    // 将索引句柄加入管理
    std::string ix_name = ix_manager_->get_index_name(tab_name, index_cols);
    ihs_[ix_name] = std::move(ih);

    flush_meta();
}

/**
 * @description: 删除索引
 * @param {string&} tab_name 表名称
 * @param {vector<string>&} col_names 索引包含的字段名称
 * @param {Context*} context
 */
void SmManager::drop_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context) {
    TabMeta &tab = db_.get_table(tab_name);

    // 找到索引元数据
    auto index_it = tab.get_index_meta(col_names);

    // 关闭并删除索引文件
    std::string ix_name = ix_manager_->get_index_name(tab_name, index_it->cols);
    auto ih_it = ihs_.find(ix_name);
    if (ih_it != ihs_.end()) {
        ix_manager_->close_index(ih_it->second.get());
        ihs_.erase(ih_it);
    }
    ix_manager_->destroy_index(tab_name, index_it->cols);

    // 从元数据中删除索引
    tab.indexes.erase(index_it);

    flush_meta();
}

/**
 * @description: 删除索引
 * @param {string&} tab_name 表名称
 * @param {vector<ColMeta>&} 索引包含的字段元数据
 * @param {Context*} context
 */
void SmManager::drop_index(const std::string& tab_name, const std::vector<ColMeta>& cols, Context* context) {
    TabMeta &tab = db_.get_table(tab_name);

    // 构建 col_names 用于查找
    std::vector<std::string> col_names;
    for (auto &col : cols) {
        col_names.push_back(col.name);
    }

    auto index_it = tab.get_index_meta(col_names);

    std::string ix_name = ix_manager_->get_index_name(tab_name, index_it->cols);
    auto ih_it = ihs_.find(ix_name);
    if (ih_it != ihs_.end()) {
        ix_manager_->close_index(ih_it->second.get());
        ihs_.erase(ih_it);
    }
    ix_manager_->destroy_index(tab_name, index_it->cols);

    tab.indexes.erase(index_it);

    flush_meta();
}

/**
 * @description: 查看表上的索引信息
 * @param {string&} tab_name 表名称
 * @param {Context*} context
 */
void SmManager::show_index_from(const std::string& tab_name, Context* context) {
    TabMeta &tab = db_.get_table(tab_name);

    std::fstream outfile;
    outfile.open("output.txt", std::ios::out | std::ios::app);

    for (auto &index : tab.indexes) {
        std::string col_list = "(";
        for (size_t i = 0; i < index.cols.size(); ++i) {
            if (i > 0) col_list += ",";
            col_list += index.cols[i].name;
        }
        col_list += ")";

        outfile << "| " << tab_name << " | unique | " << col_list << " |\n";

        std::string output = "| " + tab_name + " | unique | " + col_list + " |\n";
        memcpy(context->data_send_ + *(context->offset_), output.c_str(), output.length());
        *(context->offset_) += output.length();
    }

    outfile.close();
}