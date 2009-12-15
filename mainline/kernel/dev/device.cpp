/*
 * /kernel/dev/device.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/device.h>

Device::Device(Type type, u32 unit, u32 classID)
{
	devType = type;
	devUnit = unit;
	devClassID = classID;
}

/**************************************************************
 * Character device
 */

ChrDevice::ChrDevice(Type type, u32 unit, u32 classID) :
	Device(type, unit, classID)
{

}

Device::IOStatus
ChrDevice::Getc(u8 *c)
{
	return IOS_NOTSPRT;
}

Device::IOStatus
ChrDevice::Putc(u8 c)
{
	return IOS_NOTSPRT;
}

int
ChrDevice::Read(u8 *buf, u32 size)
{
	int got = 0;
	while (size) {
		IOStatus s = Getc(buf);
		if (s == IOS_NODATA) {
			break;
		}
		if (s == IOS_PENDING) {
			got++;
			break;
		}
		if (s != IOS_OK) {
			return -s;
		}
		got++;
		buf++;
		size--;
	}
	return got;
}

int
ChrDevice::Write(u8 *buf, u32 size)
{
	int got = 0;
	while (size) {
		IOStatus s = Putc(*buf);
		if (s == IOS_NODATA) {
			break;
		}
		if (s == IOS_PENDING) {
			got++;
			break;
		}
		if (s != IOS_OK) {
			return -s;
		}
		got++;
		buf++;
		size--;
	}
	return got;
}

/**************************************************************
 * Block device
 */

BlkDevice::BlkDevice(Type type, u32 unit, u32 classID) :
	Device(type, unit, classID)
{

}

int
BlkDevice::Strategy(IOBuf *buf)
{
	return 0;
}

/**************************************************************
 * Device manager class implementation
 */

/* system global device manager instance */
DeviceManager devMan;

DeviceManager::DeviceRegistrator::DeviceRegistrator(const char *devClass, Device::Type type,
	DeviceFactory factory, void *factoryArg)
{
	devMan.RegisterClass(devClass, type, factory, factoryArg);
}

DeviceManager::DeviceManager()
{
	Initialize();

}

void DeviceManager::Initialize()
{
	if (isInitialized == INIT_MAGIC) {
		return;
	}
	TREE_INIT(devTree);
	isInitialized = INIT_MAGIC;
}

Device *
DeviceManager::CreateDevice(DevClass *p)
{
	u32 unit = AllocateUnit(p);
	return p->factory(p->type, unit, p->id, p->factoryArg);
}

Device *
DeviceManager::CreateDevice(const char *devClass)
{
	u32 id = GetClassID(devClass);
	return CreateDevice(id);
}

Device *
DeviceManager::CreateDevice(u32 devClassID)
{
	DevClass *p = TREE_FIND(devClassID, DevClass, node, devTree);
	if (!p) {
		panic("Attempted to create device of non-existing class (0x%lx)", devClassID);
	}
	return CreateDevice(p);
}

/* returns registered device class ID, zero if error */
u32
DeviceManager::RegisterClass(const char *devClass, Device::Type type,
	DeviceFactory factory, void *factoryArg)
{
	/* our constructor can be called after DeviceRegistrator constructors */
	if (isInitialized != INIT_MAGIC) {
		Initialize();
	}
	u32 id = GetClassID(devClass);
	DevClass *found = TREE_FIND(id, DevClass, node, devTree);
	if (found) {
		if (!strcmp(devClass, found->name)) {
			panic("Registering duplicated device classes: '%s'", devClass);
		}
		panic("Device class hash collision: h('%s')=h('%s')=0x%08lx",
			devClass, found->name, id);
	}
	DevClass *p = ALLOC(DevClass, 1);
	if (!p) {
		panic("Device class registration failed, no memory");
	}
	klog(KLOG_DEBUG, "Registering device class '%s' (id = 0x%08lX)", devClass, id);
	p->id = id;
	p->node.key = id;
	p->name = strdup(devClass);
	p->type = type;
	p->factory = factory;
	p->factoryArg = factoryArg;
	p->availUnit = 0;
	TREE_ADD(node, p, devTree);
	return id;
}

u32
DeviceManager::AllocateUnit(DevClass *p)
{
	u32 unit = p->availUnit;
	p->availUnit++;
	return unit;
}

u32
DeviceManager::AllocateUnit(u32 devClassID)
{
	DevClass *p = TREE_FIND(devClassID, DevClass, node, devTree);
	if (!p) {
		panic("Allocating unit for non-existing device class");
	}
	return AllocateUnit(p);
}

char *
DeviceManager::GetClass(u32 devClassID)
{
	DevClass *p = TREE_FIND(devClassID, DevClass, node, devTree);
	if (!p) {
		klog(KLOG_WARNING, "Requesting class for none-existing device class ID");
		return 0;
	}
	return p->name;
}
