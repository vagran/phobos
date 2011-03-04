# /phobos/etc/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
# All rights reserved.
# See COPYING file for copyright details.

include $(PHOBOS_ROOT)/make/makevar.mak

INSTALL_DIR = /etc

FILES = test.txt

.PHONY: all install clean

all:

clean:

install: $(FILES)
	$(foreach FILE,$(FILES),$(INSTALL) -D $(FILE) \
		$(INSTALL_ROOT)/$(TARGET)$(INSTALL_DIR)/$(notdir $(FILE));)
	