# /kernel/make/make.mak
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
# All rights reserved.
# See COPYING file for copyright details.

TOOLS_BIN = $(PHOBOS_BUILD_TOOLS)/bin/
TOOLS_PREFIX = x86_64-phobos-elf-

export CC = $(TOOLS_BIN)$(TOOLS_PREFIX)gcc
export LD = $(TOOLS_BIN)$(TOOLS_PREFIX)ld
export STRIP = $(TOOLS_BIN)$(TOOLS_PREFIX)strip
export AR = $(TOOLS_BIN)$(TOOLS_PREFIX)ar
export OBJCOPY = $(TOOLS_BIN)$(TOOLS_PREFIX)objcopy
export INSTALL = install
export DD = dd
export CP = cp
export MOUNT = mount
export UMOUNT = umount
export DF = df
export SUDO_ASKPASS = /usr/bin/ssh-askpass
export SUDO = sudo
export MKE2FS = mke2fs

export NAT_CC = gcc
export NAT_LD = gcc

export APP_RUNTIME_LIB_NAME = apprt
export APP_RUNTIME_LIB_DIR = $(PHOBOS_ROOT)/lib/startup
export APP_RUNTIME_LIB = $(APP_RUNTIME_LIB_DIR)/build/$(TARGET)/lib$(APP_RUNTIME_LIB_NAME).a

export SL_RUNTIME_LIB_NAME = slrt.so
export SL_RUNTIME_LIB_DIR = $(PHOBOS_ROOT)/lib/sl_startup
export SL_RUNTIME_LIB = $(SL_RUNTIME_LIB_DIR)/build/$(TARGET)/$(SL_RUNTIME_LIB_NAME)

export COMMON_LIB_DIR = $(PHOBOS_ROOT)/lib/common
export COMMON_LIB_NAME = common
export COMMON_LIB = $(COMMON_LIB_DIR)/build/$(TARGET)/lib$(COMMON_LIB_NAME).a

export USER_LIB_DIR = $(PHOBOS_ROOT)/lib/user
export USER_LIB_NAME = user
export USER_LIB = $(USER_LIB_DIR)/build/$(TARGET)/lib$(USER_LIB_NAME).a

export RT_LINKER_DIR = /bin
export RT_LINKER_NAME = rt_linker

export LIBS_INSTALL_DIR = /usr/lib

export INSTALL_ROOT = $(PHOBOS_ROOT)/install

# Default load address for binary executables
export DEF_LOAD_ADDRESS = 0x1000

INCLUDE_DIRS = $(PHOBOS_ROOT)/kernel $(PHOBOS_ROOT)/kernel/sys $(PHOBOS_ROOT)/include
INCLUDE_FLAGS += $(foreach dir,$(INCLUDE_DIRS),-I$(dir))

NAT_INCLUDE_DIRS = /usr/include
NAT_INCLUDE_FLAGS = $(foreach dir,$(NAT_INCLUDE_DIRS),-I$(dir))

# kernel physical load address
export KERNEL_LOAD_ADDRESS = 0x100000
# kernel virtual mapping address
export KERNEL_ADDRESS =	0x78000000

MFS_IMAGE_NAME = mfs_image
MFS_IMAGE = $(PHOBOS_ROOT)/mfs/image/$(TARGET)/$(MFS_IMAGE_NAME)
MFS_BLOCK_SIZE = 1024
MFS_IMAGE_LABEL = PhobOS_MFS
# MFS image size in kilobytes
MFS_IMAGE_SIZE = 8000
