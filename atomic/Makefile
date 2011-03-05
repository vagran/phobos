# /Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
# All rights reserved.
# See COPYING file for copyright details.

SUBDIRS= kernel

include $(PHOBOS_ROOT)/make/make.mak

clean:
	rm -rf $(INSTALL_ROOT)
	$(MAKE) -C mfs clean
