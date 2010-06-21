# /kernel/make/make.mak
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright ©AST 2009. Written by Artemy Lebedev.

include $(PHOBOS_ROOT)/make/makevar.mak

COMPILE_FLAGS = -pipe -Werror -Wall -fno-stack-protector -fno-default-inline

COMPILE_FLAGS_CXX = -fno-exceptions -fno-rtti
COMPILE_FLAGS_C =
COMPILE_FLAGS_ASM = -DASSEMBLER
# XXX -static is temporal
LINK_FLAGS = -static -nodefaultlibs -nostartfiles -nostdinc -nostdinc++ \
	--no-omagic -z common-page-size=0x1000

ifdef APP
LINK_SCRIPT = $(PHOBOS_ROOT)/make/link.app.ld
BINARY_NAME = $(APP)
export PROFILE_NAME = APP
IS_PROFILE_ROOT = 1
LINK_FILES += $(APP_RUNTIME_LIB)
REQUIRE_RUNTIME_LIB = 1
else ifdef OBJ
BINARY_NAME = $(OBJ)
export PROFILE_NAME = OBJ
IS_PROFILE_ROOT = 1
LINK_FLAGS += -r
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
BINARY = $(OBJ_DIR)/$(BINARY_NAME)
endif

#TARGET variable must be either DEBUG or RELEASE
ifndef TARGET
export TARGET = RELEASE
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

SUBDIRS_TARGET = $(foreach item,$(SUBDIRS),$(item).dir)

ifdef LINK_SCRIPT
LINK_FLAGS += -T $(LINK_SCRIPT)
endif

.PHONY: all clean install FORCE $(SUBDIRS_TARGET)

all: $(BINARY) $(SUBDIRS_TARGET) $(OBJ_DIR) $(OBJS)

ifdef BINARY
$(BINARY): $(SUBDIRS_TARGET) $(OBJ_DIR) $(LINK_SCRIPT) $(LINK_FILES) $(OBJS)
	$(LD) $(LINK_FLAGS) $(LINK_FILES) -o $@ $(wildcard $(OBJ_DIR)/*.o)
endif

$(OBJ_DIR)/%.o: %.c
	$(CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_C) -o $@ $<

$(OBJ_DIR)/%.o: %.cpp
	$(CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_CXX) -o $@ $<

$(OBJ_DIR)/%.o: %.S
	$(CC) -c $(INCLUDE_FLAGS) $(COMPILE_FLAGS) $(COMPILE_FLAGS_ASM) -o $@ $<

$(SUBDIRS_TARGET):
	$(MAKE) -C $(patsubst %.dir,%,$@) $(MAKECMDGOALS)

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

ifdef REQUIRE_RUNTIME_LIB
$(APP_RUNTIME_LIB):
	$(MAKE) -C $(PHOBOS_ROOT)/lib/startup
endif

ifdef INSTALL_DIR
install: $(BINARY) $(SUBDIRS_TARGET)
	$(INSTALL) -D $(BINARY) $(INSTALL_ROOT)/$(TARGET)$(INSTALL_DIR)/$(BINARY_NAME)
else
install: $(SUBDIRS_TARGET)
endif
