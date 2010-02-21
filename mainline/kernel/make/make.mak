# /kernel/make/make.mak
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright ©AST 2009. Written by Artemy Lebedev.

include $(PHOBOS_ROOT)/make/makevar.mak

COMPILE_FLAGS = -pipe -Werror -Wall -pipe -DKERNEL -fno-stack-protector -fno-default-inline \
				-DLOAD_ADDRESS=$(LOAD_ADDRESS) \
				-DKERNEL_ADDRESS=$(KERNEL_ADDRESS)
COMPILE_FLAGS_CXX = -fno-exceptions -fno-rtti
COMPILE_FLAGS_C =
COMPILE_FLAGS_ASM = -DASSEMBLER
LINK_FLAGS = -static -nodefaultlibs -nostartfiles -nostdinc -nostdinc++
LINK_SCRIPT = $(KERNROOT)/make/link.ld
AR_FLAGS = rcs

INCLUDE_DIRS = $(KERNROOT) $(KERNROOT)/sys
INCLUDE_FLAGS = $(foreach dir,$(INCLUDE_DIRS),-I$(dir))

#TARGET variable must be either DEBUG or RELEASE
ifndef TARGET
TARGET = RELEASE
export TARGET
endif

ifeq ($(TARGET),RELEASE)
COMPILE_FLAGS += -O2
else ifeq ($(TARGET),DEBUG)
COMPILE_FLAGS += -ggdb3 -DDEBUG -O0 -DENABLE_TRACING -DSERIAL_DEBUG -DDEBUG_MALLOC
else
$(error Target not supported: $(TARGET))
endif

COMPILE_DIR = $(KERNROOT)/build
OBJ_DIR = $(COMPILE_DIR)/$(TARGET)

SUBDIRS_TARGET = $(foreach item,$(SUBDIRS),$(item).dir)

ifdef MAKELIB
LIB_FILE = $(OBJ_DIR)/lib$(MAKELIB).a
endif

ifeq ($(DO_RAMDISK),1)
RAMDISK_SIZE = $(shell expr $(MFS_IMAGE_SIZE) '*' $(MFS_BLOCK_SIZE))
LINK_FILES += $(MFS_IMAGE)
RAMDISK_FILE = $(MFS_IMAGE)
else
RAMDISK_SIZE = 0
endif

ifeq ($(MAKEIMAGE),1)
IMAGE = $(OBJ_DIR)/kernel
RMBUILD = rm -Rf $(COMPILE_DIR)
else
SRCS = $(wildcard *.S *.c *.cpp)
OBJS_LOCAL = $(subst .S,.o,$(subst .c,.o,$(subst .cpp,.o,$(SRCS))))
OBJS = $(foreach obj,$(OBJS_LOCAL),$(OBJ_DIR)/$(obj))
endif

.PHONY: all clean FORCE $(SUBDIRS_TARGET)

all: $(OBJ_DIR) $(OBJS) $(IMAGE) $(SUBDIRS_TARGET) $(LIB_FILE)

ifeq ($(MAKEIMAGE),1)
$(IMAGE): $(OBJ_DIR) $(SUBDIRS_TARGET) $(LINK_SCRIPT) $(RAMDISK_FILE)
	$(LD) $(LINK_FLAGS) --defsym LOAD_ADDRESS=$(LOAD_ADDRESS) \
		--defsym KERNEL_ADDRESS=$(KERNEL_ADDRESS) \
		-T $(LINK_SCRIPT) -o $@ $(wildcard $(OBJ_DIR)/*.o) $(LINK_FILES)
endif

ifeq ($(DO_RAMDISK),1)
$(RAMDISK_FILE):
	$(MAKE) -C $(PHOBOS_ROOT)/mfs all
endif

$(SUBDIRS_TARGET):
	$(MAKE) -C $(patsubst %.dir,%,$@) $(MAKECMDGOALS)

$(COMPILE_DIR):
	if [ ! -d $@ ]; then mkdir $@; fi

$(OBJ_DIR): $(COMPILE_DIR)
	if [ ! -d $@ ]; then mkdir $@; fi


$(OBJ_DIR)/%.o: %.c
	$(CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_C) -o $@ $<

$(OBJ_DIR)/%.o: %.cpp
	$(CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_CXX) -o $@ $<

$(OBJ_DIR)/%.o: %.S
	$(CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_ASM) -o $@ $<

ifdef MAKELIB
$(LIB_FILE): $(OBJS)
	$(AR) $(AR_FLAGS) $@ $^
endif

clean: $(SUBDIRS_TARGET)
	$(RMBUILD)

install:
