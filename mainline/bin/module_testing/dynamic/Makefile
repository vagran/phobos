# /phobos/bin/module_testing/dynamic/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
# All rights reserved.
# See COPYING file for copyright details.

APP = module_test_dyn
INSTALL_DIR = /bin

INCLUDES += .. $(PHOBOS_ROOT)/lib/module_testing

VPATH = ../static
SRCS += module_test.cpp proc_args.cpp

LIBS += module_testing module_testing2

COMPILE_FLAGS += -DMT_DYNAMIC -DMT_NAME="\"Module testing (dyn)\""

include $(PHOBOS_ROOT)/make/make.mak
