/*
 * /phobos/kernel/fs/fs.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

DeviceFS::FSRegistrator::FSRegistrator(const char *id, const char *desc,
	Factory factory, Prober prober, void *arg)
{
	RegisterFilesystem(id, desc, factory, prober, arg);
}

ListHead DeviceFS::fsReg;

u32 DeviceFS::numFsReg;

DeviceFS::DeviceFS(BlkDevice *dev)
{
	dev->AddRef();
	this->dev = dev;
	status = -1;
}

DeviceFS::~DeviceFS()
{
	dev->Release();
}

int
DeviceFS::RegisterFilesystem(const char *id, const char *desc, Factory factory,
	Prober prober, void *arg)
{
	FSEntry *p = NEW(FSEntry);
	assert(p);
	p->id = strdup(id);
	p->desc = desc ? strdup(desc) : 0;
	p->factory = factory;
	p->prober = prober;
	p->arg = arg;
	LIST_ADD(list, p, fsReg);
	numFsReg++;
	return 0;
}

DeviceFS::FSEntry *
DeviceFS::GetFS(const char *name)
{
	FSEntry *p;

	LIST_FOREACH(FSEntry, list, p, fsReg) {
		if (!strcmp(name, p->id)) {
			return p;
		}
	}
	return 0;
}

DeviceFS *
DeviceFS::Create(BlkDevice *dev, const char *name)
{
	FSEntry *fse;
	if (name) {
		fse = GetFS(name);
		if (!fse) {
			klog(KLOG_WARNING, "File system not found: '%s'", name);
			return 0;
		}
		if (fse->prober(dev, fse->arg)) {
			klog(KLOG_WARNING, "File system do not match the content: '%s' on %s%lu",
				name, dev->GetClass(), dev->GetUnit());
		}
	} else {
		int found = 0;
		LIST_FOREACH(FSEntry, list, fse, fsReg) {
			if (!fse->prober(dev, fse->arg)) {
				found = 1;
				break;
			}
			if (!found) {
				klog(KLOG_WARNING, "File system not recognized on %s%lu",
					dev->GetClass(), dev->GetUnit());
				return 0;
			}
		}
	}
	return fse->factory(dev, fse->arg);
}
