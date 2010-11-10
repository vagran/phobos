# /phobos/etc/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright (c)AST 2009. Written by Artemy Lebedev.

include $(PHOBOS_ROOT)/make/makevar.mak

INSTALL_DIR = /etc

FILES = test.txt

.PHONY: all install clean

all:

clean:

install: $(FILES)
	$(foreach FILE,$(FILES),$(INSTALL) -D $(FILE) \
		$(INSTALL_ROOT)/$(TARGET)$(INSTALL_DIR)/$(notdir $(FILE));)
	