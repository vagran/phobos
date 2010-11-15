# /Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright �AST 2009. Written by Artemy Lebedev.

SUBDIRS= lib bin etc kernel

include $(PHOBOS_ROOT)/make/make.mak

clean:
	rm -rf $(INSTALL_ROOT)
	$(MAKE) -C mfs clean
