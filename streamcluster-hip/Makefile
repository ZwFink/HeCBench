CC        = hipcc
OPTIMIZE  = yes
DEBUG     = no
PROFILE   = yes


# Standard flags
CFLAGS := $(EXTRA_CFLAGS) -std=c++14 -Wall

# Linker flags
LDFLAGS = -lm

ifeq ($(PROFILE),yes)
  CFLAGS += -DPROFILE_TMP
endif

# Enable checksum and output file
ifeq ($(DEBUG),yes)
  CFLAGS += -g 
endif

# Optimization Flags
ifeq ($(OPTIMIZE),yes)
  CFLAGS += -O3
endif

SRC = streamcluster.cu
EXE = streamcluster

$(EXE):$(SRC) kernel.h streamcluster.h  streamcluster_cl.h
	$(CC) $(CFLAGS) $(SRC) -o $(EXE) $(LDFLAGS)	

run: $(EXE)
	./$(EXE) 10 20 256 65536 65536 1000 none output.txt 1 

clean: 
	rm -f $(EXE) output.txt result*
