#===============================================================================
# User Options
#===============================================================================

# Compiler can be set below, or via environment variable
CC        = hipcc
OPTIMIZE  = yes
DEBUG     = no

#===============================================================================
# Program name & source code list
#===============================================================================

program = main

obj = main.o force_kernel.o

#===============================================================================
# Sets Flags
#===============================================================================

# Standard Flags
CFLAGS := $(EXTRA_CFLAGS) -std=c++14 -I../mcmd-cuda \
          -D_MWAITXINTRIN_H_INCLUDED -D_FORCE_INLINES -D__STRICT_ANSI__ \
          -DGPU

# Linker Flags
LDFLAGS = 

# Debug Flags
ifeq ($(DEBUG),yes)
  CFLAGS += -g -DDEBUG
  LDFLAGS  += -g
endif

# Optimization Flags
ifeq ($(OPTIMIZE),yes)
  CFLAGS += -O3
endif

#===============================================================================
# Targets to Build
#===============================================================================

$(program): $(obj)
	$(CC) $(obj) $(CFLAGS) -o $@ $(LDFLAGS)

main.o: ../mcmd-cuda/main.cpp
	$(CC) $(CFLAGS) -c $< -o $@

force_kernel.o: force_kernel.cu
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(program) $(obj)
	rm -rf ../mcmd-cuda/dataset/*.pdb
	rm -rf ../mcmd-cuda/dataset/*.xyz
	rm -rf ../mcmd-cuda/dataset/*.bak
	rm -rf ../mcmd-cuda/dataset/*.dat

run: $(program)
	cd ../mcmd-cuda/dataset && ../../mcmd-hip/$(program) mcmd.inp && cd ../../mcmd-hip
