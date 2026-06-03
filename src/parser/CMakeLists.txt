# 使用 Bison/Flex 重新生成解析器文件以确保与 yacc.y/lex.l 完全一致
find_package(BISON REQUIRED)
find_package(FLEX REQUIRED)

BISON_TARGET(MyParser yacc.y ${CMAKE_CURRENT_BINARY_DIR}/yacc.tab.cpp
    COMPILE_FLAGS "-Wno-conflicts-sr -Wno-conflicts-rr -Wno-other")
FLEX_TARGET(MyScanner lex.l ${CMAKE_CURRENT_BINARY_DIR}/lex.yy.cpp)

add_flex_bison_dependency(MyScanner MyParser)

# 将生成的源文件加入库
add_library(parser STATIC
    ${BISON_MyParser_OUTPUTS}
    ${FLEX_MyScanner_OUTPUTS}
    ast.cpp
)

# 设置 include 路径以找到生成的头文件
target_include_directories(parser PUBLIC ${CMAKE_CURRENT_BINARY_DIR})

add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser parser)
add_test(NAME test_parser COMMAND test_parser
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
