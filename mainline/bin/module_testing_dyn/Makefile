# /phobos/bin/module_testing_dyn/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright (c)AST 2009. Written by Artemy Lebedev.

APP = module_test_dyn
INSTALL_DIR = /bin

INCLUDES += ../module_testing $(PHOBOS_ROOT)/lib/module_testing

VPATH = ../module_testing
SRCS += module_test.cpp proc_args.cpp

LIBS += module_testing

COMPILE_FLAGS += -DMT_DYNAMIC -DMT_NAME="\"Module testing (dyn)\""

include $(PHOBOS_ROOT)/make/make.mak
