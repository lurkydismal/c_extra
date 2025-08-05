set(LLVM_LINK_COMPONENTS support)

add_clang_executable(iterate_struct
  iterate_struct.cpp
  )
target_link_libraries(iterate_struct
  PRIVATE
  clangAST
  clangASTMatchers
  clangBasic
  clangFrontend
  clangSerialization
  clangTooling
  )
