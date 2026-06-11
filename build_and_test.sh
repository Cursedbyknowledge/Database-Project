#!/bin/bash
# RMDB 项目构建与测试脚本
# 在 WSL Ubuntu 环境中运行

set -e

PROJECT_DIR="/home/zyc/db2026"
BUILD_DIR="$PROJECT_DIR/build"

echo "============================================"
echo "  RMDB 数据库竞赛项目 - 构建与测试脚本"
echo "============================================"

# 1. 环境检查
echo ""
echo "[1/5] 检查编译环境..."
if ! command -v g++ &> /dev/null; then
    echo "错误: g++ 未安装，请执行:"
    echo "  sudo apt install -y build-essential cmake flex bison libreadline-dev"
    exit 1
fi
echo "  g++ 版本: $(g++ --version | head -1)"
echo "  cmake 版本: $(cmake --version | head -1)"

# 2. 重新生成解析器 (lex/yacc)
echo ""
echo "[2/5] 重新生成 SQL 解析器..."
cd "$PROJECT_DIR/src/parser"
if command -v bison &> /dev/null && command -v flex &> /dev/null; then
    echo "  运行 bison..."
    bison -d -o yacc.tab.cpp yacc.y
    echo "  运行 flex..."
    flex -o lex.yy.cpp lex.l
    echo "  解析器重新生成完成"
else
    echo "  警告: bison/flex 未安装，使用现有解析器文件"
    echo "  如需重新生成请执行: sudo apt install -y flex bison"
fi

# 3. CMake 配置
echo ""
echo "[3/5] 配置 CMake 构建..."
mkdir -p "$BUILD_DIR"
cd "$BUILD_DIR"
cmake "$PROJECT_DIR" -DCMAKE_BUILD_TYPE=Debug

# 4. 编译
echo ""
echo "[4/5] 编译项目..."
make -j$(nproc)

echo ""
echo "[5/5] 编译完成！"
echo ""

# 5. 运行单元测试
echo "============================================"
echo "  运行单元测试"
echo "============================================"
echo ""
./bin/unit_test --gtest_filter="LRUReplacerTest.*" 2>&1 | tail -5
echo ""
./bin/unit_test --gtest_filter="BufferPoolManagerTest.*" 2>&1 | tail -5
echo ""
./bin/unit_test --gtest_filter="StorageTest.*" 2>&1 | tail -5
echo ""
./bin/unit_test --gtest_filter="RecordManagerTest.*" 2>&1 | tail -5
echo ""

echo "============================================"
echo "  所有测试完成！"
echo "============================================"
echo ""
echo "使用说明:"
echo "  启动服务端: ./build/bin/rmdb <数据库名>"
echo "  连接服务端: nc localhost 8765"
echo "  示例: ./build/bin/rmdb execution_test_db"
echo ""
