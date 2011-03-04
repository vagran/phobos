# /mfs/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
# All rights reserved.
# See COPYING file for copyright details.

include $(PHOBOS_ROOT)/make/makevar.mak

ifdef KERNROOT
unexport KERNROOT
endif

.PHONY: all install system_install

all: $(MFS_IMAGE)

install:

MFS_MOUNT = mfs_mount

$(MFS_IMAGE): system_install
	$(INSTALL) -d $(dir $(MFS_IMAGE))
	if [ ! -d $(MFS_MOUNT) ]; then mkdir $(MFS_MOUNT); fi
	$(DD) if=/dev/zero of=$(MFS_IMAGE) bs=$(MFS_BLOCK_SIZE) count=$(MFS_IMAGE_SIZE)
	$(MKE2FS) -F -m 0 -b $(MFS_BLOCK_SIZE) -L $(MFS_IMAGE_LABEL) $(MFS_IMAGE) $(MFS_IMAGE_SIZE)
	$(SUDO) $(MOUNT) -t ext2 -o loop $(MFS_IMAGE) $(MFS_MOUNT)
	$(CP) -R $(INSTALL_ROOT)/$(TARGET)/* $(MFS_MOUNT); \
	if [ $$? -ne 0 ]; then \
		$(SUDO) $(UMOUNT) $(MFS_MOUNT); \
		exit 1; \
	fi
	$(DF) -h $(MFS_MOUNT)
	$(SUDO) $(UMOUNT) $(MFS_MOUNT)
	$(OBJCOPY) -S -I binary -O elf32-i386 -B i386 --rename-section .data=.ramdisk $(MFS_IMAGE)

system_install:
	$(MAKE) -C $(PHOBOS_ROOT) install
	
clean:
	rm -rf $(MFS_MOUNT) image
