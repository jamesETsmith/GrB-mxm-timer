# -*- Makefile -*-

-include make.inc
CC ?= gcc
CFLAGS ?= -std=c11 -ggdb -O3 -fopenmp
LDFLAGS ?= -fopenmp
#LDLIBS ?= -lgraphblas

OBJS = GrB-mxm-timer.o cmdline.o generator.o prng.o io.o globals.o
ifndef TARGET_MWX
OBJS += hooks.o
endif

OBJS_ELGEN = el-generator.o el-generator-cmdline.o generator.o prng.o globals.o

CPPFLAGS += -Irandom123/include
LDLIBS += -lm

ifdef TARGET_MWX
TARGET_EXECUTABLE = GrB-mxm-timer.mwx
else
TARGET_EXECUTABLE = GrB-mxm-timer el-generator
endif

all: $(TARGET_EXECUTABLE)

GrB-mxm-timer: $(OBJS)
el-generator: $(OBJS_ELGEN)

%.mwx: %
	cp $< $@

cmdline.c cmdline.h : cmdline.ggo
	gengetopt < $^

el-generator-cmdline.c el-generator-cmdline.h : el-generator-cmdline.ggo
	gengetopt -F el-generator-cmdline < $^

GrB-mxm-timer.o: GrB-mxm-timer.c globals.h generator.h prng.h
el-generator.o: el-generator.c globals.h generator.h prng.h
cmdline.o: cmdline.c
el-generator-cmdline.o: el-generator-cmdline.c
generator.o: generator.c globals.h prng.h compat.h
prng.o: prng.c prng.h globals.h
io.o: io.c io.h globals.h compat.h
globals.o: globals.c globals.h
ifndef TARGET_MWX
hooks.o: hooks.c hooks.h
endif

# Ugly hack.
GrB-mxm-timer.mwx: GrB-mxm-timer

.PHONY: clean
clean:
	rm -f GrB-mxm-timer.mwx GrB-mxm-timer $(OBJS)
