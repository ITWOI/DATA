
LLVM_BIN =/home/fff000/Documents/llvm/build/Release+Asserts/bin/


CC =$(LLVM_BIN)/clang
# Use llvm-gcc instead of clang

LLVM_config = $(LLVM_BIN)/llvm-config

CFLAGS= -nostdinc -isystem $(shell llvm-config --libdir)/clang/$(shell llvm-config --version)/include/  -I/usr/src/linux-headers-$(shell uname -r)/arch/x86/include -I/lib/modules/$(shell uname -r)/build/arch/x86/include/generated -I/lib/modules/$(shell uname -r)/build/include/ -Iinclude -I/usr/src/linux-headers-$(shell uname -r)/arch/x86/include/uapi -I/lib/modules/$(shell uname -r)/build/arch/x86/include/generated/uapi -I/usr/src/linux-headers-$(shell uname -r)/include/uapi -I/lib/modules/$(shell uname -r)/build/include/generated/uapi -include /usr/src/linux-headers-$(shell uname -r)/include/linux/kconfig.h -I/lib/modules/$(shell uname -r)/build/ubuntu/include -I/home/fff000/Documents/klee/include\
-D__KERNEL__ -Wall -Wundef -Wstrict-prototypes -Wno-trigraphs -fno-strict-aliasing -fno-common -Wno-format-security -std=gnu89 -O0 -m64 -mno-mmx -mno-sse -mtune=generic -mno-red-zone -mcmodel=kernel -funit-at-a-time -fstack-protector -DCONFIG_X86_X32_ABI -DCONFIG_AS_CFI=1 -DCONFIG_AS_CFI_SIGNAL_FRAME=1 -DCONFIG_AS_CFI_SECTIONS=1 -DCONFIG_AS_FXSAVEQ=1 -DCONFIG_AS_AVX=1 -DCONFIG_AS_AVX2=1 -pipe -Wno-sign-compare \
-fno-asynchronous-unwind-tables -mno-sse -mno-mmx -mno-sse2 -mno-3dnow -fno-omit-frame-pointer -fno-optimize-sibling-calls -pg -DCC_USING_FENTRY -Wdeclaration-after-statement -Wno-pointer-sign -fno-strict-overflow -Werror=implicit-int -Werror=strict-prototypes -DMODULE -g

NAME =  
SUBPATHLENGTH = 8
DIR = $(shell pwd)
all:    buildCFG



 
 
buildCFG:$(NAME).bc

# Use llvm-gcc instead of clang
$(NAME).bc:$(NAME).c
	$(CC) $(CFLAGS) -c -emit-llvm $(DIR)/$(NAME).c -o $(DIR)/$(NAME).bc
	$(CC) $(CFLAGS) -c -emit-ast $(DIR)/$(NAME).c -o $(DIR)/$(NAME).ast
clean:
	rm -f *.bc *.ast
