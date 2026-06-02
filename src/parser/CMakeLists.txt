# 使用预生成的解析器文件，不调用 Bison/Flex 重新生成
# 这是为了确保解析器与 yacc.y/lex.l 完全一致（避免不同 Bison 版本的差异）
set(SOURCES yacc.tab.cpp lex.yy.cpp ast.cpp)
add_library(parser STATIC ${SOURCES})

add_executable(test_parser test_parser.cpp)
target_link_libraries(test_parser parser)
add_test(NAME test_parser COMMAND test_parser
        WORKING_DIRECTORY ${CMAKE_RUNTIME_OUTPUT_DIRECTORY})
