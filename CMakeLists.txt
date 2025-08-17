set(LLVM_LINK_COMPONENTS
    Support
)

add_clang_executable(c_extra
    main.cpp
    arguments_parse.cpp
    cextra_frontend.cpp
    cextra_ast_consumer.cpp
    iterate_arguments.cpp
    iterate_enum.cpp
    iterate_struct_union.cpp
)

target_link_libraries(c_extra
    PRIVATE
    clangAST
    clangASTMatchers
    clangBasic
    clangFrontend
    clangSerialization
    clangTooling
)
