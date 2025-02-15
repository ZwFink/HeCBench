CC          = icpx
OPTIMIZE    = yes
DEBUG       = no
DEVICE      = gpu

# Matrix library path
MATRIX_DIR = ./meschach_lib

# Standard flags
CFLAGS := $(EXTRA_CFLAGS) -std=c++11 -Wall -I$(MATRIX_DIR)

# Linker flags
LDFLAGS = -lm


# Debug Flags
ifeq ($(DEBUG),yes)
  CFLAGS += -g -DDEBUG
endif

ifeq ($(DEBUG),yes)
  CFLAGS += -g  
  OUTPUT = -DOUTPUT
endif

# Optimization Flags
ifeq ($(OPTIMIZE),yes)
  CFLAGS += -O3
endif

ifeq ($(DEVICE),gpu)
  CFLAGS +=-fiopenmp -fopenmp-targets=spir64 -D__STRICT_ANSI__
else
  CFLAGS +=-qopenmp
endif

leukocyte: detect_main.o \
	avilib.o \
	track_ellipse.o \
	track_ellipse_gpu.o \
	find_ellipse.o \
	misc_math.o \
	helper.o \
	$(MATRIX_DIR)/meschach.a
	$(CC) $(CFLAGS) -o leukocyte avilib.o \
		track_ellipse.o \
		track_ellipse_gpu.o \
		find_ellipse.o \
		misc_math.o \
		detect_main.o \
		helper.o \
		$(MATRIX_DIR)/meschach.a $(LDFLAGS)

%.o: %.[c]
	$(CC) $(OUTPUT) $(CFLAGS) $< -c

detect_main.o: detect_main.c \
               kernel_GICOV.h kernel_dilated.h \
               track_ellipse.h helper.h misc_math.h

find_ellipse.o: find_ellipse.c find_ellipse.h avilib.h misc_math.h

track_ellipse.o: track_ellipse.c find_ellipse.h $(MATRIX_DIR)/matrix.h avilib.h helper.h

track_ellipse_gpu.o: track_ellipse_gpu.c track_ellipse.h kernel_IMGVF.h


# Cleanup everything, then clean everything except the static library
$(MATRIX_DIR)/meschach.a:
	cd $(MATRIX_DIR); make cleanup; make all CC=$(CC); make clean

run: leukocyte
	./leukocyte ../data/leukocyte/testfile.avi 100

clean:
	rm -f *.o *~ leukocyte 
	cd $(MATRIX_DIR); make cleanup
