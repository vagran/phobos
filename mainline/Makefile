# /Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright ©AST 2009. Written by Artemy Lebedev.

SUBDIRS= kernel bin lib

include $(PHOBOS_ROOT)/make/make.mak

clean:
	rm -rf $(INSTALL_ROOT)
	$(MAKE) -C mfs clean
