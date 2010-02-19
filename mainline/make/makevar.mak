# /kernel/make/make.mak
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright ©AST 2009. Written by Artemy Lebedev.

TOOLS_BIN = $(PHOBOS_BUILD_TOOLS)/bin/
TOOLS_PREFIX = i786-phobos-elf-

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
export SUDO_ASKPASS = /usr/bin/gksudo
export SUDO = sudo
export MKE2FS = mke2fs

export APP_RUNTIME_LIB_NAME = crt.o
export APP_RUNTIME_LIB = $(PHOBOS_ROOT)/lib/startup/build/$(TARGET)/$(APP_RUNTIME_LIB_NAME)

export INSTALL_ROOT = $(PHOBOS_ROOT)/install

INCLUDE_DIRS = $(PHOBOS_ROOT)/kernel $(PHOBOS_ROOT)/kernel/sys $(PHOBOS_ROOT)/include
INCLUDE_FLAGS = $(foreach dir,$(INCLUDE_DIRS),-I$(dir))

MFS_IMAGE_NAME = mfs_image
MFS_IMAGE = $(PHOBOS_ROOT)/mfs/$(TARGET)/$(MFS_IMAGE_NAME)
MFS_BLOCK_SIZE = 1024
MFS_IMAGE_LABEL = PhobOS_MFS
# MFS image size in kilobytes
MFS_IMAGE_SIZE = 4096
