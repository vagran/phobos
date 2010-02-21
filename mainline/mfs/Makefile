# /mfs/Makefile
# $Id$
#
# This file is a part of PhobOS operating system.
# Copyright ©AST 2009. Written by Artemy Lebedev.

include $(PHOBOS_ROOT)/make/makevar.mak

.PHONY: all system_install

all: $(MFS_IMAGE)

MFS_MOUNT = mfs_mount

$(MFS_IMAGE): system_install
	$(INSTALL) -d $(dir $(MFS_IMAGE))
	if [ ! -d $(MFS_MOUNT) ]; then mkdir $(MFS_MOUNT); fi
	$(DD) if=/dev/zero of=$(MFS_IMAGE) bs=$(MFS_BLOCK_SIZE) count=$(MFS_IMAGE_SIZE)
	$(MKE2FS) -F -m 0 -b $(MFS_BLOCK_SIZE) -L $(MFS_IMAGE_LABEL) $(MFS_IMAGE) $(MFS_IMAGE_SIZE)
	$(SUDO) $(MOUNT) -t ext2 -o loop $(MFS_IMAGE) $(MFS_MOUNT)
	$(CP) -R $(INSTALL_ROOT)/* $(MFS_MOUNT); $(SUDO) $(UMOUNT) $(MFS_MOUNT)
	$(OBJCOPY) -S -I binary -O elf32-i386 -B i386 --rename-section .data=.ramdisk $(MFS_IMAGE)

system_install:
	$(MAKE) -C $(PHOBOS_ROOT) install
	
clean:
	rm -rf $(MFS_MOUNT) $(TARGET)
