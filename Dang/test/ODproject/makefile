CC := clang
CFLAGS := -g -Xclang -load -Xclang /home/fff000/Documents/nodang/llvm/build/OD/libODPass.so

tests : main.o other.o
	$(CC) -o $@ *.o

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

notests : main.o other.o
	$(CC) -o $@ *.o

%.o: %.cpp
	$(CC) -g -c -o $@ $<



bcTest : main.bc other.bc
	llvm-link -o tests.bc *.bc

%.bc: %.c
	$(CC) -emit-llvm -c -o $@ $<

clean :
	rm -f *.o *.bc *.ll
