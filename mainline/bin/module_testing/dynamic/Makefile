# /phobos/bin/module_testing/dynamic/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright (c)AST 2009. Written by Artemy Lebedev.

APP = module_test_dyn
INSTALL_DIR = /bin

INCLUDES += .. $(PHOBOS_ROOT)/lib/module_testing

VPATH = ../static
SRCS += module_test.cpp proc_args.cpp

LIBS += module_testing module_testing2

COMPILE_FLAGS += -DMT_DYNAMIC -DMT_NAME="\"Module testing (dyn)\""

include $(PHOBOS_ROOT)/make/make.mak
