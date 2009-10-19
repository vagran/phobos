# /kernel/make/unit_test.mak
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright ©AST 2009. Written by Artemy Lebedev.

export NAT_CC= gcc
export NAT_LD= gcc

EXECUTABLE= utrun

INCLUDE_DIRS= $(KERNROOT) $(KERNROOT)/sys
INCLUDE_FLAGS= $(foreach dir,$(INCLUDE_DIRS),-I$(dir))

COMPILE_FLAGS= -g -Werror -Wall -pipe

LIBS= -lstdc++

COMPILE_FLAGS_CXX= -fno-rtti -fno-exceptions
COMPILE_FLAGS_C=
COMPILE_FLAGS_ASM= -DASSEMBLER

LINK_FLAGS=

OBJ_DIR= $(KERNROOT)/unit_test/obj
PROG= $(OBJ_DIR)/$(EXECUTABLE)
UNIT_OBJ_DIR= $(KERNROOT)/unit_test/unit_obj

SRCS= $(wildcard *.S *.c *.cpp)
OBJS_LOCAL= $(subst .S,.o,$(subst .c,.o,$(subst .cpp,.o,$(SRCS))))
OBJS= $(foreach obj,$(OBJS_LOCAL),$(OBJ_DIR)/$(obj))

UNIT_SRCS_LOCAL= $(foreach src,$(UNIT_SRCS),../$(src))
UNIT_SRCS_DIRS= $(dir $(UNIT_SRCS_LOCAL))
UNIT_OBJS_ROOT= $(subst .S,.o,$(subst .c,.o,$(subst .cpp,.o,$(UNIT_SRCS))))
UNIT_OBJS= $(foreach obj,$(UNIT_OBJS_ROOT),$(UNIT_OBJ_DIR)/$(notdir $(obj)))


$(OBJ_DIR):
	if [ ! -d $@ ]; then mkdir $@; fi
$(UNIT_OBJ_DIR):
	if [ ! -d $@ ]; then mkdir $@; fi

# main.cpp must be compiled without kernel include paths to avoid conflicts
$(OBJ_DIR)/main.o: main.cpp
	$(NAT_CC) -c $(COMPILE_FLAGS) $(COMPILE_FLAGS_CXX) -o $@ $<

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


.PHONY: all clean

all: $(PROG)
	
clean:
	rm -rf $(OBJ_DIR) $(UNIT_OBJ_DIR)
