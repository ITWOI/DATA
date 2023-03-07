make clean
make bcTest
opt -load /home/fff000/Documents/llvm/Release+Asserts/lib/LLVMNoDang.so -NoDang tests.bc -o tests_tar.bc
