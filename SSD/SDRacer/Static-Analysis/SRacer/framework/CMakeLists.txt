
set(LLVM_LINK_COMPONENTS support)
set(LLVM_USED_LIBS clangTooling clangBasic clangAST)

add_clang_executable(huawei-checker
  main.cpp
  ASTManager.cpp
  BasicChecker.cpp
  CallGraph.cpp
  Common.cpp
  Config.cpp
  )

target_link_libraries(huawei-checker
  clangAST
  clangBasic
  clangDriver
  clangFrontend
  clangRewriteFrontend
  clangStaticAnalyzerFrontend
  clangTooling
  LLVMTarget
  LLVMX86AsmParser
  )
