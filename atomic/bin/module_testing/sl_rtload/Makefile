# /phobos/bin/module_testing/sl_rtload/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
# All rights reserved.
# See COPYING file for copyright details.

APP = sl_rtload_test

INCLUDES += .. $(PHOBOS_ROOT)/lib/module_testing

VPATH = ../static
SRCS += module_test.cpp proc_args.cpp

COMPILE_FLAGS += -DMT_DYNAMIC -DMT_NAME="\"Module testing (sl_rtload)\""

EXPORTABLE = 1

include $(PHOBOS_ROOT)/make/make.mak
