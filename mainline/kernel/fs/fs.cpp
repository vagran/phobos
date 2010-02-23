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
