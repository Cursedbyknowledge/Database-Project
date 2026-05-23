/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#include <sys/stat.h>
#include <fstream>

#include "transaction/transaction_manager.h"
#include "sm_manager.h"

RmManager *SmManager::get_rm_manager() const {
    return rm_manager_;
}

IxManager *SmManager::get_ix_manager() const {
    return ix_manager_;
}

bool SmManager::is_dir(const std::string &db_name) const {
    struct stat st;
    return stat(db_name.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void SmManager::create_db(const std::string &db_name) {
    if (is_dir(db_name)) {
        throw DatabaseExistsError(db_name);
    }
    // 为数据库创建一个子目录
    if (mkdir(db_name.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH) < 0) {
        throw UnixError();
    }
    // 目录创建后，启动存储管理器的相关流程
    if (chdir(db_name.c_str()) < 0) {
        throw UnixError();
    }
    // 创建数据库的系统目录文件
    DbMeta new_db;
    new_db.name_ = db_name;

    // 分配一个磁盘文件用来记录数据库的元数据
    disk_manager_->create_file(META_DATA_NAME);
    int meta_fd = disk_manager_->open_file(META_DATA_NAME);

    // 将内存中的新的数据库元数据写入磁盘元数据文件中
    Json::Value root;
    root["name"] = new_db.name_;
    root["tabs"] = Json::arrayValue;
    Json::FastWriter writer;
    std::string buf = writer.write(root);
    if (write(meta_fd, buf.c_str(), buf.size()) == -1) {
        throw UnixError();
    }

    // 创建日志文件
    disk_manager_->create_file(LOG_FILE_NAME);
}

void SmManager::drop_db(const std::string &db_name) {}

void SmManager::open_db(const std::string &db_name) {
    if (!is_dir(db_name)) {
        throw DatabaseNotFoundError(db_name);
    }
    // 进入数据库目录
    if (chdir(db_name.c_str()) < 0) {
        throw UnixError();
    }
    // 打开元数据文件，并将内容读取到db_中
    int meta_fd = disk_manager_->open_file(META_DATA_NAME);
    int file_len = disk_manager_->get_file_size(META_DATA_NAME);
    std::string buf;
    buf.resize(file_len);
    if (read(meta_fd, buf.data(), file_len) < 0) {
        throw UnixError();
    }

    Json::Reader reader;
    Json::Value root;
    if (!reader.parse(buf, root)) {
        throw UnixError();  // TODO: 应考虑Fail on reading db.meta文件
    }

    db_.name_ = root["name"].asString();
    
    // Load table metadata
    for (const auto &tab_value : root["tabs"]) {
        TabMeta tab;
        tab.name_ = tab_value["name"].asString();
        
        // Load column metadata
        for (const auto &col_value : tab_value["cols"]) {
            ColMeta col;
            col.tab_name = col_value["tab_name"].asString();
            col.name = col_value["name"].asString();
            col.type = static_cast<ColType>(col_value["type"].asInt());
            col.len = col_value["len"].asInt();
            col.offset = col_value["offset"].asInt();
            tab.cols.push_back(col);
        }
        
        // Load index metadata
        for (const auto &idx_value : tab_value["indexes"]) {
            IndexMeta idx;
            idx.tab_name = idx_value["tab_name"].asString();
            idx.col_num = idx_value["col_num"].asInt();
            idx.col_tot_len = idx_value["col_tot_len"].asInt();
            for (const auto &idx_col : idx_value["cols"]) {
                ColMeta col;
                col.tab_name = idx_col["tab_name"].asString();
                col.name = idx_col["name"].asString();
                col.type = static_cast<ColType>(idx_col["type"].asInt());
                col.len = idx_col["len"].asInt();
                col.offset = idx_col["offset"].asInt();
                idx.cols.push_back(col);
            }
            tab.indexes.push_back(idx);
        }
        
        db_.tabs_[tab.name_] = tab;
    }

    // 打开表格相关的文件句柄
    for (auto &entry : db_.tabs_) {
        const auto &tab_name = entry.first;
        fhs_.emplace(tab_name, rm_manager_->open_file(tab_name));
        
        // Open index handles
        for (auto &index : entry.second.indexes) {
            std::string index_name = ix_manager_->get_index_name(tab_name, index.cols);
            ihs_.emplace(index_name, ix_manager_->open_index(tab_name, index.cols));
        }
    }
}

void SmManager::close_db() {
    // 清空db_中维护的表结构
    db_.tabs_.clear();
    
    // 关闭所有表格的文件句柄
    for (auto &entry : fhs_) {
        rm_manager_->close_file(entry.second.get());
    }
    fhs_.clear();
    
    // 关闭所有索引的文件句柄
    for (auto &entry : ihs_) {
        ix_manager_->close_index(entry.second.get());
    }
    ihs_.clear();
    
    // 关闭磁盘管理器和缓存池
    buffer_pool_manager_->flush_all_pages(BUFFER_POOL_SIZE);
    disk_manager_->close_file(disk_manager_->open_file(META_DATA_NAME));
}

void SmManager::show_tables(Context *context) {
    // 打印表头
    RecordPrinter rec_printer(1);
    rec_printer.print_separator(context);
    rec_printer.print_record({"Tables_in_" + db_.name_}, context);
    rec_printer.print_separator(context);
    
    // 打印每个表的名字
    for (const auto &entry : db_.tabs_) {
        rec_printer.print_record({entry.first}, context);
    }
    rec_printer.print_separator(context);
}

void SmManager::desc_table(const std::string &tab_name, Context *context) {
    TabMeta &tab = db_.get_table(tab_name);
    
    // 打印表头
    RecordPrinter rec_printer(6);  // Field, Type, Null, Key, Default, Extra
    rec_printer.print_separator(context);
    rec_printer.print_record({"Field", "Type", "Null", "Key", "Default", "Extra"}, context);
    rec_printer.print_separator(context);
    
    // 打印每一列的元数据
    for (const auto &col : tab.cols) {
        std::string type_str = coltype2str(col.type);
        rec_printer.print_record({col.name, type_str, "YES", "", "NULL", ""}, context);
    }
    rec_printer.print_separator(context);
}

void SmManager::create_table(const std::string &tab_name, const std::vector<ColDef> &col_defs, Context *context) {
    if (db_.is_table(tab_name)) {
        throw TableExistsError(tab_name); 
    }
    
    // 在db_中记录meta信息
    TabMeta tab;
    tab.name_ = tab_name;
    
    // 计算每一列的偏移和总记录大小
    int offset = 0;
    FrmMeta frm;
    frm.tab_name = tab_name;
    frm.col_num = col_defs.size();
    frm.key_num = 0;
    
    for (const auto &col_def : col_defs) {
        // 设置列元数据
        ColMeta col;
        col.tab_name = tab_name;
        col.name = col_def.name;
        col.type = col_def.type;
        col.len = col_def.len;
        col.offset = offset;
        offset += col_def.len;
        
        // 检查唯一索引定义
        if (col_def.is_unique) {
            tab.indexes.push_back({tab_name, 1, {col}, col.len});
            frm.key_num++;
        }
        
        frm.cols.push_back(col);
        tab.cols.push_back(col);
    }
    
    // 创建记录文件
    fhs_.emplace(tab_name, rm_manager_->create_file(tab_name, offset));
    
    // 创建索引文件
    for (auto &index : tab.indexes) {
        std::string index_name = ix_manager_->get_index_name(tab_name, index.cols);
        ihs_.emplace(index_name, ix_manager_->create_index(tab_name, index.cols));
    }
    
    // 将表元数据记录到db_中
    db_.tabs_[tab_name] = tab;
    
    // 将当前所有表格的元数据刷入磁盘
    flush_meta();
}

void SmManager::drop_table(const std::string &tab_name, Context *context) {
    if (!db_.is_table(tab_name)) {
        throw TableNotFoundError(tab_name);
    }
    
    // 获取表元数据
    TabMeta &tab = db_.get_table(tab_name);
    
    // 删除索引文件
    for (auto &index : tab.indexes) {
        std::string index_name = ix_manager_->get_index_name(tab_name, index.cols);
        ix_manager_->close_index(ihs_.at(index_name).get());
        ix_manager_->destroy_index(tab_name, index.cols);
        ihs_.erase(index_name);
    }
    
    // 删除记录文件
    rm_manager_->close_file(fhs_.at(tab_name).get());
    rm_manager_->destroy_file(tab_name);
    fhs_.erase(tab_name);
    
    // 从db_中删除表元数据
    db_.tabs_.erase(tab_name);
    
    // 刷新元数据到磁盘
    flush_meta();
}

void SmManager::create_index(const std::string &tab_name, const std::vector<std::string> &col_names, Context *context) {}

void SmManager::drop_index(const std::string &tab_name, const std::vector<std::string> &col_names, Context *context) {}

void SmManager::flush_meta() {
    Json::Value root;
    root["name"] = db_.name_;
    root["tabs"] = Json::arrayValue;
    
    for (auto &entry : db_.tabs_) {
        Json::Value tab_value;
        tab_value["name"] = entry.second.name_;
        
        // 存储列信息
        Json::Value cols_value;
        for (auto &col : entry.second.cols) {
            Json::Value col_value;
            col_value["tab_name"] = col.tab_name;
            col_value["name"] = col.name;
            col_value["type"] = static_cast<int>(col.type);
            col_value["len"] = col.len;
            col_value["offset"] = col.offset;
            cols_value.append(col_value);
        }
        tab_value["cols"] = cols_value;
        
        // 存储索引信息
        Json::Value index_values;
        for (auto &index : entry.second.indexes) {
            Json::Value index_value;
            index_value["tab_name"] = index.tab_name;
            index_value["col_num"] = index.col_num;
            index_value["col_tot_len"] = index.col_tot_len;
            Json::Value idx_cols_value;
            for (auto &col : index.cols) {
                Json::Value col_value;
                col_value["tab_name"] = col.tab_name;
                col_value["name"] = col.name;
                col_value["type"] = static_cast<int>(col.type);
                col_value["len"] = col.len;
                col_value["offset"] = col.offset;
                idx_cols_value.append(col_value);
            }
            index_value["cols"] = idx_cols_value;
            index_values.append(index_value);
        }
        tab_value["indexes"] = index_values;
        root["tabs"].append(tab_value);
    }
    
    Json::FastWriter writer;
    std::string buf = writer.write(root);
    
    // 打开元数据文件并写入
    int meta_fd = disk_manager_->open_file(META_DATA_NAME);
    lseek(meta_fd, 0, SEEK_SET);
    if (ftruncate(meta_fd, 0) < 0) {
        throw UnixError();
    }
    if (write(meta_fd, buf.c_str(), buf.size()) == -1) {
        throw UnixError();
    }
}