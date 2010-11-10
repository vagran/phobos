# /kernel/make/make.mak
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright �AST 2009. Written by Artemy Lebedev.

include $(PHOBOS_ROOT)/make/makevar.mak

ifndef LOAD_ADDRESS
LOAD_ADDRESS = $(DEF_LOAD_ADDRESS)
endif

COMPILE_FLAGS += -pipe -Werror -Wall -fno-stack-protector  -fno-builtin

COMPILE_FLAGS_CXX += -fno-exceptions -fno-rtti -Wno-invalid-offsetof \
	-fno-default-inline
COMPILE_FLAGS_C +=
COMPILE_FLAGS_ASM += -DASSEMBLER
LINK_FLAGS += -nodefaultlibs -nostartfiles -nostdinc -nostdinc++ \
	--no-omagic -z common-page-size=0x1000 --defsym LOAD_ADDRESS=$(LOAD_ADDRESS)

#TARGET variable must be either DEBUG or RELEASE
ifndef TARGET
export TARGET = RELEASE
endif

########
ifdef APP
ifndef LINK_SCRIPT
LINK_SCRIPT = $(PHOBOS_ROOT)/make/link.app.ld
endif
BINARY_NAME = $(APP)
export PROFILE_NAME = APP
IS_PROFILE_ROOT = 1
LINK_FILES += $(APP_RUNTIME_LIB) 
ifeq ($(STATIC),1)
LINK_FILES += $(COMMON_LIB) $(USER_LIB)
else
LINK_FILES += $(subst .a,.sl,$(COMMON_LIB) $(USER_LIB))
LINK_FLAGS += -Bdynamic
endif
REQUIRE_RUNTIME_LIB = 1

########
else ifdef OBJ
BINARY_NAME = $(OBJ)
export PROFILE_NAME = OBJ
IS_PROFILE_ROOT = 1
LINK_FLAGS += -r

########
else ifdef LIB
BINARY_NAME = lib$(LIB).a 
ifneq ($(STATIC),1)
BINARY_NAME += lib$(LIB).sl
endif
export PROFILE_NAME = LIB
IS_PROFILE_ROOT = 1
ifneq ($(NO_INSTALL),1)
ifndef INSTALL_DIR
INSTALL_DIR = $(LIBS_INSTALL_DIR)
endif
endif

########
else ifndef SUBDIRS
$(error Build profile not specified)
endif

ifeq ($(STATIC),1)
LINK_FLAGS += -static
endif

ifdef IS_PROFILE_ROOT
export PROFILE_ROOT = $(CURDIR)
RMBUILD = rm -rf $(COMPILE_DIR)
endif

ifdef PROFILE_NAME
COMPILE_DIR = $(PROFILE_ROOT)/build
OBJ_DIR = $(COMPILE_DIR)/$(TARGET)
endif

ifdef BINARY_NAME
BINARY = $(foreach bin,$(BINARY_NAME),$(OBJ_DIR)/$(bin))
endif

ifeq ($(TARGET),RELEASE)
COMPILE_FLAGS += -O2
else ifeq ($(TARGET),DEBUG)
COMPILE_FLAGS += -ggdb3 -DDEBUG -O0 -DENABLE_TRACING -DSERIAL_DEBUG -DDEBUG_MALLOC
else
$(error Target not supported: $(TARGET))
endif

SRCS = $(wildcard *.S *.c *.cpp)
OBJS_LOCAL = $(subst .S,.o,$(subst .c,.o,$(subst .cpp,.o,$(SRCS))))
OBJS = $(foreach obj,$(OBJS_LOCAL),$(OBJ_DIR)/$(obj))
OBJS_SO = $(subst .o,.so,$(OBJS))

SUBDIRS_TARGET = $(foreach item,$(SUBDIRS),$(item).dir)

ifdef LINK_SCRIPT
LINK_FLAGS += -T $(LINK_SCRIPT)
endif

.PHONY: all clean install FORCE $(SUBDIRS_TARGET) $(BINARY_NAME)

all: $(BINARY) $(SUBDIRS_TARGET) $(OBJ_DIR) $(OBJS)

ifdef BINARY
# Phony file name targets depends on absolute paths of the files
$(BINARY_NAME): % : $(OBJ_DIR)/%

# The executable binary
$(filter-out %.a %.sl, $(BINARY)): $(SUBDIRS_TARGET) $(OBJ_DIR) $(LINK_SCRIPT) \
	$(LINK_FILES) $(OBJS)
	$(LD) $(LINK_FLAGS) --start-group $(LINK_FILES) --end-group \
	--dynamic-linker $(RT_LINKER_DIR)/$(RT_LINKER_NAME) -o $@ $(OBJS)

# Archive files for static linkage
$(filter %.a, $(BINARY)): $(SUBDIRS_TARGET) $(OBJ_DIR) $(LINK_SCRIPT) \
	$(LINK_FILES) $(OBJS)
	$(AR) rcs $@ $(OBJS)
	
# Shared library
$(filter %.sl, $(BINARY)): $(SUBDIRS_TARGET) $(OBJ_DIR) $(LINK_SCRIPT) \
	$(LINK_FILES) $(OBJS_SO)
	$(LD) $(LINK_FLAGS) -fpic -shared --start-group $(LINK_FILES) --end-group \
	-soname=$(@F) -o $@ $(OBJS_SO)
endif

# Build dependencies - static and dynamic libraries
$(filter %.a %.sl, $(LINK_FILES)):
	$(MAKE) -C $(abspath $(@D)/../..) $(@F)

# Relocatable objects
$(OBJ_DIR)/%.o: %.c
	$(CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_C) -o $@ $<

$(OBJ_DIR)/%.o: %.cpp
	$(CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_CXX) -o $@ $<

$(OBJ_DIR)/%.o: %.S
	$(CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_ASM) -o $@ $<

# Position-independent objects
$(OBJ_DIR)/%.so: %.c
	$(CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_C) -fpic -D__PIC -o $@ $<

$(OBJ_DIR)/%.so: %.cpp
	$(CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_CXX) -fpic -D__PIC -o $@ $<

$(OBJ_DIR)/%.so: %.S
	$(CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_ASM) -fpic -D__PIC -o $@ $<

$(SUBDIRS_TARGET):
	@$(MAKE) -C $(patsubst %.dir,%,$@) $(MAKECMDGOALS)

$(COMPILE_DIR):
	if [ ! -d $@ ]; then mkdir $@; fi

$(OBJ_DIR): $(COMPILE_DIR)
	if [ ! -d $@ ]; then mkdir $@; fi

ifdef IS_PROFILE_ROOT
clean: $(SUBDIRS_TARGET)
	$(RMBUILD)
else
clean: $(SUBDIRS_TARGET)
endif

ifdef INSTALL_DIR
INSTALLED_BINARY = $(filter-out %.a %.o, $(BINARY))
ifneq ($(INSTALLED_BINARY),)
install: $(INSTALLED_BINARY) $(SUBDIRS_TARGET)
	$(foreach bin,$(INSTALLED_BINARY),$(INSTALL) -D $(bin) \
		$(INSTALL_ROOT)/$(TARGET)$(INSTALL_DIR)/$(notdir $(bin));)
endif
else
install: $(SUBDIRS_TARGET)
endif
