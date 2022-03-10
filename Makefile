#
# Makefile
#

CC ?= gcc
CXX ?= g++
PROJ_DIR ?= ${shell pwd}

WARNINGS ?= -Wall -Wextra \
			-Wshadow -Wundef -Wmaybe-uninitialized \
			-Wno-unused-function -Wno-error=strict-prototypes -Wpointer-arith -fno-strict-aliasing -Wno-error=cpp -Wuninitialized \
			-Wno-unused-parameter -Wno-missing-field-initializers -Wno-format-nonliteral -Wno-cast-qual -Wunreachable-code -Wno-switch-default  \
			-Wreturn-type -Wmultichar -Wformat-security -Wno-ignored-qualifiers -Wno-error=pedantic -Wno-sign-compare -Wno-error=missing-prototypes -Wdouble-promotion -Wclobbered -Wdeprecated  \
			-Wempty-body -Wshift-negative-value -Wstack-usage=4096 \
			-Wtype-limits -Wsizeof-pointer-memaccess -Wpointer-arith

ASAN_FLAGS ?= -fsanitize=address -fsanitize=undefined -fno-omit-frame-pointer

LIBS ?= -lm

CFLAGS ?= -O0 -g $(WARNINGS) $(ASAN_FLAGS)
CXXFLAGS ?= $(CFLAGS)
LDFLAGS ?= $(LIBS) $(ASAN_FLAGS)
BIN = demo

#Collect the files to compile
MAINSRC = ./main.c

CSRCS += cache_manager/cache_manager.c

OBJEXT ?= .o

AOBJS = $(ASRCS:.S=$(OBJEXT))
COBJS = $(CSRCS:.c=$(OBJEXT))
CXXOBJS = $(CXXSRCS:.cpp=$(OBJEXT))

MAINOBJ = $(MAINSRC:.c=$(OBJEXT))

OBJS = $(AOBJS) $(COBJS) $(CXXOBJS)

## MAINOBJ -> OBJFILES

all: default

%.o: %.c
	@$(CC)  $(CFLAGS) -c $< -o $@
	@echo "CC $<"

%.o: %.cpp
	@$(CXX)  $(CXXFLAGS) -c $< -o $@
	@echo "CXX $<"
    
default: $(AOBJS) $(COBJS) $(CXXOBJS) $(MAINOBJ)
	$(CXX) -o $(BIN) $(MAINOBJ) $(AOBJS) $(COBJS) $(CXXOBJS) $(LDFLAGS)

clean: 
	rm -f $(BIN) $(AOBJS) $(COBJS) $(CXXOBJS) $(MAINOBJ)