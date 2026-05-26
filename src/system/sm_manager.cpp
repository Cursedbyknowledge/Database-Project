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

bool SmManager::is_dir(const std::string& db_name) {
    struct stat st;
    return stat(db_name.c_str(), &st) == 0 && S_ISDIR(st.st_mode);
}

void SmManager::create_db(const std::string& db_name) {
    if (is_dir(db_name)) {
        throw DatabaseExistsError(db_name);
    }
    std::string cmd = "mkdir " + db_name;
    if (system(cmd.c_str()) < 0) {
        throw UnixError();
    }
    if (chdir(db_name.c_str()) < 0) {
        throw UnixError();
    }
    DbMeta *new_db = new DbMeta();
    new_db->name_ = db_name;

    std::ofstream ofs(DB_META_NAME);

    ofs << *new_db;

    delete new_db;

    disk_manager_->create_file(LOG_FILE_NAME);

    if (chdir("..") < 0) {
        throw UnixError();
    }
}

void SmManager::drop_db(const std::string& db_name) {
    if (!is_dir(db_name)) {
        throw DatabaseNotFoundError(db_name);
    }
    std::string cmd = "rm -r " + db_name;
    if (system(cmd.c_str()) < 0) {
        throw UnixError();
    }
}

void SmManager::open_db(const std::string& db_name) {
    if (chdir(db_name.c_str()) < 0) {
        throw UnixError();
    }
    std::ifstream ifs(DB_META_NAME);
    ifs >> db_;
    for (auto &entry : db_.tabs_) {
        fhs_.emplace(entry.first, rm_manager_->open_file(entry.first));
        auto &indexes = entry.second.indexes;
        for (auto it = indexes.begin(); it != indexes.end(); ) {
            std::string ix_name = ix_manager_->get_index_name(entry.first, it->cols);
            if (!disk_manager_->is_file(ix_name)) {
                it = indexes.erase(it);
            } else {
                try {
                    ihs_.emplace(ix_name, ix_manager_->open_index(entry.first, it->cols));
                    ++it;
                } catch (...) {
                    it = indexes.erase(it);
                }
            }
        }
    }
    if (chdir("..") < 0) {
        throw UnixError();
    }
}

void SmManager::flush_meta() {
    std::ofstream ofs(DB_META_NAME);
    ofs << db_;
}

void SmManager::close_db() {
    for (auto &entry : fhs_) {
        rm_manager_->close_file(entry.second.get());
    }
    fhs_.clear();
    for (auto &entry : ihs_) {
        ix_manager_->close_index(entry.second.get());
    }
    ihs_.clear();
    flush_meta();
}

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

void SmManager::desc_table(const std::string& tab_name, Context* context) {
    TabMeta &tab = db_.get_table(tab_name);

    std::vector<std::string> captions = {"Field", "Type", "Index"};
    RecordPrinter printer(captions.size());
    printer.print_separator(context);
    printer.print_record(captions, context);
    printer.print_separator(context);
    for (auto &col : tab.cols) {
        std::vector<std::string> field_info = {col.name, coltype2str(col.type), col.index ? "YES" : "NO"};
        printer.print_record(field_info, context);
    }
    printer.print_separator(context);
}

void SmManager::create_table(const std::string& tab_name, const std::vector<ColDef>& col_defs, Context* context) {
    if (db_.is_table(tab_name)) {
        throw TableExistsError(tab_name);
    }
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
    int record_size = curr_offset;
    rm_manager_->create_file(tab_name, record_size);
    db_.tabs_[tab_name] = tab;
    fhs_.emplace(tab_name, rm_manager_->open_file(tab_name));

    flush_meta();
}

void SmManager::drop_table(const std::string& tab_name, Context* context) {
    auto it = fhs_.find(tab_name);
    if (it == fhs_.end()) {
        throw TableNotFoundError(tab_name);
    }
    try {
        auto tab_it = db_.tabs_.find(tab_name);
        if (tab_it != db_.tabs_.end()) {
            for (auto &index : tab_it->second.indexes) {
                std::string ix_name = ix_manager_->get_index_name(tab_name, index.cols);
                try {
                    auto ih_it = ihs_.find(ix_name);
                    if (ih_it != ihs_.end()) {
                        ix_manager_->close_index(ih_it->second.get());
                        ihs_.erase(ih_it);
                    }
                    ix_manager_->destroy_index(tab_name, index.cols);
                } catch (...) {}
            }
        }
    } catch (...) {}
    rm_manager_->close_file(it->second.get());
    fhs_.erase(it);
    rm_manager_->destroy_file(tab_name);
    db_.tabs_.erase(tab_name);
    flush_meta();
}

void SmManager::create_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context) {
    TabMeta &tab = db_.get_table(tab_name);

    if (tab.is_index(col_names)) {
        throw RMDBError("Index already exists");
    }

    std::vector<ColMeta> index_cols;
    for (auto &col_name : col_names) {
        auto col_it = tab.get_col(col_name);
        index_cols.push_back(*col_it);
    }

    ix_manager_->create_index(tab_name, index_cols);

    IndexMeta index_meta;
    index_meta.tab_name = tab_name;
    index_meta.col_num = (int)col_names.size();
    index_meta.col_tot_len = 0;
    index_meta.cols = index_cols;
    for (auto &col : index_cols) {
        index_meta.col_tot_len += col.len;
    }
    tab.indexes.push_back(index_meta);

    try {
        auto ih = ix_manager_->open_index(tab_name, index_cols);
        auto fh = fhs_.at(tab_name).get();
        char key_buf[index_meta.col_tot_len];

        for (RmScan scan(fh); !scan.is_end(); scan.next()) {
            auto rid = scan.rid();
            auto rec = fh->get_record(rid, context);
            int offset = 0;
            for (auto &col : index_cols) {
                memcpy(key_buf + offset, rec->data + col.offset, col.len);
                offset += col.len;
            }
            ih->insert_entry(key_buf, rid, nullptr);
        }

        std::string ix_name = ix_manager_->get_index_name(tab_name, index_cols);
        ihs_[ix_name] = std::move(ih);
    } catch (...) {
        ix_manager_->destroy_index(tab_name, index_cols);
        tab.indexes.pop_back();
        throw;
    }

    flush_meta();
}

void SmManager::drop_index(const std::string& tab_name, const std::vector<std::string>& col_names, Context* context) {
    TabMeta &tab = db_.get_table(tab_name);

    auto index_it = tab.get_index_meta(col_names);
    std::vector<ColMeta> index_cols = index_it->cols;

    std::string ix_name = ix_manager_->get_index_name(tab_name, index_cols);
    auto ih_it = ihs_.find(ix_name);
    if (ih_it != ihs_.end()) {
        ix_manager_->close_index(ih_it->second.get());
        ihs_.erase(ih_it);
    }

    ix_manager_->destroy_index(tab_name, index_cols);
    tab.indexes.erase(index_it);
    flush_meta();
}

void SmManager::drop_index(const std::string& tab_name, const std::vector<ColMeta>& cols, Context* context) {
    TabMeta &tab = db_.get_table(tab_name);

    std::vector<std::string> col_names;
    for (auto &col : cols) {
        col_names.push_back(col.name);
    }
    drop_index(tab_name, col_names, context);
}

void SmManager::show_index(const std::string& tab_name, Context* context) {
    TabMeta &tab = db_.get_table(tab_name);

    std::fstream outfile;
    outfile.open("output.txt", std::ios::out | std::ios::app);

    for (auto &index : tab.indexes) {
        outfile << "| " << tab_name << " | unique | (";
        for (size_t i = 0; i < index.cols.size(); i++) {
            if (i > 0) outfile << ",";
            outfile << index.cols[i].name;
        }
        outfile << ") |\n";
    }

    outfile.close();
}