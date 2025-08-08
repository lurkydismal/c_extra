set(LLVM_LINK_COMPONENTS
    Support
)

add_clang_executable(c_extra
    main.cpp
    arguments_parse.cpp
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
