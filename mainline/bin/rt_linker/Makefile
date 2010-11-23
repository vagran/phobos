# /phobos/bin/rt_linker/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright ©AST 2009. Written by Artemy Lebedev.

APP = $(RT_LINKER_NAME)
INSTALL_DIR = $(RT_LINKER_DIR)

STATIC = 1

RT_LINKER_LOAD_ADDRESS = 0x40000000

COMPILE_FLAGS += -DRT_LINKER_LOAD_ADDRESS=$(RT_LINKER_LOAD_ADDRESS) \
	-DRT_LINKER_DEFDIR="$(LIBS_INSTALL_DIR)"

LOAD_ADDRESS = $(RT_LINKER_LOAD_ADDRESS)

LINK_FLAGS += --defsym RT_LINKER_START_RES=$(DEF_LOAD_ADDRESS)

LINK_SCRIPTS += rt_linker.ld

LIBS = elf

INCLUDES += $(PHOBOS_ROOT)/lib/elf

include $(PHOBOS_ROOT)/make/make.mak
