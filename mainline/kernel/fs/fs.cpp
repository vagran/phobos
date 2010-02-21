/*
 * /phobos/kernel/fs/fs.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

DeviceFS::DeviceFS(BlkDevice *dev)
{
	dev->AddRef();
	this->dev = dev;
}

DeviceFS::~DeviceFS()
{
	dev->Release();
}
