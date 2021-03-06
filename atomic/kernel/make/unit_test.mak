# /kernel/make/unit_test.mak
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
# All rights reserved.
# See COPYING file for copyright details.

include $(PHOBOS_ROOT)/make/makevar.mak

EXECUTABLE = utrun

COMPILE_FLAGS = -pipe -Werror -Wall -fno-default-inline -DUNIT_TEST \
	-DKERNEL_ADDRESS=$(KERNEL_ADDRESS)

ifeq ($(TARGET),RELEASE)
COMPILE_FLAGS += -O2
else ifeq ($(TARGET),DEBUG)
COMPILE_FLAGS += -ggdb3 -DDEBUG -O0 -DENABLE_TRACING
else
$(error Target not supported: $(TARGET))
endif

LIBS = -lstdc++

COMPILE_FLAGS_CXX = -fno-rtti -fno-exceptions
COMPILE_FLAGS_C =
COMPILE_FLAGS_ASM = -DASSEMBLER

LINK_FLAGS =

OBJ_DIR = $(KERNROOT)/unit_test/obj
PROG = $(OBJ_DIR)/$(EXECUTABLE)
UNIT_OBJ_DIR = $(KERNROOT)/unit_test/unit_obj

SRCS = $(wildcard *.S *.c *.cpp)
OBJS_LOCAL = $(subst .S,.o,$(subst .c,.o,$(subst .cpp,.o,$(SRCS))))
OBJS = $(foreach obj,$(OBJS_LOCAL),$(OBJ_DIR)/$(obj))

UNIT_SRCS_GLOBAL = $(abspath $(foreach src,$(UNIT_SRCS),../$(src)))
UNIT_SRCS_GLOBAL += $(filter-out %gcc.cpp, $(wildcard $(COMMON_LIB_DIR)/*.cpp))
UNIT_SRCS_DIRS = $(sort $(dir $(UNIT_SRCS_GLOBAL)))
UNIT_OBJS_ROOT = $(subst .S,.o,$(subst .c,.o,$(subst .cpp,.o,$(UNIT_SRCS_GLOBAL))))
UNIT_OBJS = $(foreach obj,$(UNIT_OBJS_ROOT),$(UNIT_OBJ_DIR)/$(notdir $(obj)))

.PHONY: all clean

all: $(PROG)

$(OBJ_DIR):
	if [ ! -d $@ ]; then mkdir $@; fi
$(UNIT_OBJ_DIR):
	if [ ! -d $@ ]; then mkdir $@; fi

# main.cpp must be compiled without kernel include paths to avoid conflicts
$(OBJ_DIR)/main.o: main.cpp
	$(NAT_CC) $(NAT_INCLUDE_FLAGS) -c $(COMPILE_FLAGS) $(COMPILE_FLAGS_CXX) -o $@ $<

$(OBJ_DIR)/%.o: %.c
	$(NAT_CC) $(INCLUDE_FLAGS) -c $(COMPILE_FLAGS) $(COMPILE_FLAGS_C) -o $@ $<

$(OBJ_DIR)/%.o: %.cpp
	$(NAT_CC) $(INCLUDE_FLAGS) -c $(COMPILE_FLAGS) $(COMPILE_FLAGS_CXX) -o $@ $<

$(OBJ_DIR)/%.o: %.S
	$(NAT_CC) $(INCLUDE_FLAGS) -c $(COMPILE_FLAGS) $(COMPILE_FLAGS_ASM) -o $@ $<

vpath %.c $(UNIT_SRCS_DIRS)
vpath %.cpp $(UNIT_SRCS_DIRS)
vpath %.S $(UNIT_SRCS_DIRS)

$(UNIT_OBJ_DIR)/%.o: %.c
	$(NAT_CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_C) -o $@ $<

$(UNIT_OBJ_DIR)/%.o: %.cpp
	$(NAT_CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_CXX) -o $@ $<

$(UNIT_OBJ_DIR)/%.o: %.S
	$(NAT_CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_ASM) -o $@ $<


$(PROG): $(OBJ_DIR) $(UNIT_OBJ_DIR) $(OBJS) $(UNIT_OBJS)
	$(NAT_LD) $(LINK_FLAGS) $(LIBS) -o $@ $(OBJS) $(UNIT_OBJS)
	
clean:
	rm -Rf $(OBJ_DIR) $(UNIT_OBJ_DIR)
