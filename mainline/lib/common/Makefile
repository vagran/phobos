# /kernel/lib/Tree/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright (c)AST 2009. Written by Artemy Lebedev.

ifdef KERNROOT

include ${KERNROOT}/make/make.mak

else

LIB = $(COMMON_LIB_NAME)

#//temp - test dependencies trees handling by run-time linker
LIBS += elf

include $(PHOBOS_ROOT)/make/make.mak

endif
