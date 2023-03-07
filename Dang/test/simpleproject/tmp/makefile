CC := clang
CFLAGS := -g

tests : main.o other.o
	$(CC) -o $@ *.o

%.o: %.cpp
	$(CC) $(CFLAGS) -c -o $@ $<

bcTest : main.bc other.bc
	llvm-link -o tests.bc *.bc

%.bc: %.c
	$(CC) -emit-llvm -c -o $@ $<

clean :
	rm -f *.o *.bc *.ll
