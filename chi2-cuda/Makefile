#===============================================================================
# User Options
#===============================================================================

# Compiler can be set below, or via environment variable
CC        = nvcc
OPTIMIZE  = yes
DEBUG     = no
ARCH      = sm_60

#===============================================================================
# Program name & source code list
#===============================================================================

program = chi2

source = chi2.cu

obj = $(source:.cu=.o)

#===============================================================================
# Sets Flags
#===============================================================================

# Standard Flags
CFLAGS := $(EXTRA_CFLAGS) -std=c++14 -Xcompiler -Wall -arch=$(ARCH)

# Linker Flags
LDFLAGS = 

# Debug Flags
ifeq ($(DEBUG),yes)
  CFLAGS += -g
  LDFLAGS  += -g
endif


# Optimization Flags
ifeq ($(OPTIMIZE),yes)
  CFLAGS += -O3
endif

#===============================================================================
# Targets to Build
#===============================================================================

$(program): $(obj) Makefile
	$(CC) $(CFLAGS) $(obj) -o $@ $(LDFLAGS)

%.o: %.cu reference.h Makefile
	$(CC) $(CFLAGS) -c $< -o $@

clean:
	rm -rf $(program) $(obj)

run: $(program)
	./$(program) traindata 4000 400000 2000 2000 256 1000
