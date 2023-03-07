##Prerequisites
- llvm 3.6 or 3.4
  cmake

##How to compile
  mkdir build
  cd build
  cmake -D LLVM_CONFIG=${PATH_TO_LLVM}/build/bin -D CMAKE_BUILD_TYPE=Release ..
