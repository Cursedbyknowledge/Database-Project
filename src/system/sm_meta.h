/* Copyright (c) 2023 Renmin University of China
RMDB is licensed under Mulan PSL v2.
You can use this software according to the terms and conditions of the Mulan PSL v2.
You may obtain a copy of Mulan PSL v2 at:
        http://license.coscl.org.cn/MulanPSL2
THIS SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES OF ANY KIND,
EITHER EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO NON-INFRINGEMENT,
MERCHANTABILITY OR FIT FOR A PARTICULAR PURPOSE.
See the Mulan PSL v2 for more details. */

#pragma once
#include <string>
#include "common/common.h"
#include "sm_defs.h"

struct DbMeta {
    std::string name_;
    std::map<std::string, TabMeta> tabs_;
};

struct TabMeta {
    std::string name_;
    std::vector<ColMeta> cols;
    std::vector<IndexMeta> indexes;
};

struct ColMeta {
    std::string tab_name;
    std::string name;
    ColType type;
    int len;
    int offset;
};

struct IndexMeta {
    std::string tab_name;
    int col_num;            // Number of columns indexed
    std::vector<ColMeta> cols;
    int col_tot_len;
};

struct FrmMeta {
    std::string tab_name;
    int col_num;            // Number of columns stored in the format file
    int key_num;            // Number of columns that is indexed
    std::vector<ColMeta> cols;
};