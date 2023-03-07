CC := clang
CFLAGS := -g -O0 -Xclang -load -Xclang ~/Documents/nodang/llvm/build/src/libNoDangPass.so

tests : main.o other.o
	$(CC) -o $@ *.o

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

notests : main.o other.o
	$(CC) -o $@ *.o

%.o: %.cpp
	$(CC) -g -c -o $@ $<

ast : main.ast other.ast
	find $(pwd) -name "*.ast" > astList.txt

%.ast: %.c
	$(CC) -g -O0 -emit-ast $<

bcTest : main.bc other.bc
	llvm-link -o tests.bc *.bc

%.bc: %.c
	$(CC) -g -emit-llvm -c -o $@ $<

clean :
	rm -f *.o *.bc *.ll
