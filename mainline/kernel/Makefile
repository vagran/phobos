# /kernel/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright ©AST 2009. Written by Artemy Lebedev.

KERNROOT= $(PHOBOS_ROOT)/kernel
export KERNROOT
MAKEIMAGE= 1
LOAD_ADDRESS=	0x00100000
KERNEL_ADDRESS=	0xc0000000

SUBDIRS= init lib dev mem

include $(KERNROOT)/make/make.mak
