# /phobos/bin/rt_linker/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright ©AST 2009. Written by Artemy Lebedev.

APP = $(RT_LINKER_NAME)
INSTALL_DIR = $(RT_LINKER_DIR)

STATIC = 1

RT_LINKER_LOAD_ADDRESS = 0x40000000

COMPILE_FLAGS += -DRT_LINKER_LOAD_ADDRESS=$(RT_LINKER_LOAD_ADDRESS)

LOAD_ADDRESS = $(RT_LINKER_LOAD_ADDRESS)

LIBS = elf

include $(PHOBOS_ROOT)/make/make.mak
