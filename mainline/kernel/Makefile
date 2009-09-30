# /kernel/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright ©AST 2009. Written by Artemy Lebedev.

export KERNROOT= $(PHOBOS_ROOT)/kernel
MAKEIMAGE= 1
LOAD_ADDRESS=	0x00100000
KERNEL_ADDRESS=	0xc0000000

SUBDIRS= init lib dev mem unit_test

include $(KERNROOT)/make/make.mak
