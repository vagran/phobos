/*
 * /kernel/dev/device.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");


Device::Device(Type type, u32 unit, u32 classID)
{
	devType = type;
	devUnit = unit;
	devClassID = classID;
	devState = S_DOWN;
}

Device::~Device()
{

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
	const char *desc, DeviceFactory factory, void *factoryArg)
{
	devMan.RegisterClass(devClass, type, desc, factory, factoryArg);
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

DeviceManager::DevInst *
DeviceManager::FindDevice(DevClass *p, u32 unit)
{
	u32 x = Lock();
	DevInst *di = TREE_FIND(unit, DevInst, tree, p->devTree);
	Unlock(x);
	return di;
}

Device *
DeviceManager::CreateDevice(DevClass *p, u32 unit)
{
	int unitAllocated;
	if (unit == DEF_UNIT) {
		unit = AllocateUnit(p);
		unitAllocated = 1;
	} else {
		if (FindDevice(p, unit)) {
			klog(KLOG_ERROR, "'%s' class device unit %lu already exists",
				p->name, unit);
			return 0;
		}
		unitAllocated = 0;
	}
	Device *dev = p->factory(p->type, unit, TREE_KEY(node, p), p->factoryArg);
	if (!dev) {
		if (unitAllocated) {
			ReleaseUnit(p, unit);
		}
		return 0;
	}
	Device::State state = dev->GetState();
	if (state != Device::S_UP) {
		DELETE(dev);
		if (unitAllocated) {
			ReleaseUnit(p, unit);
		}
		return 0;
	}
	DevInst *di = NEW(DevInst);
	if (!di) {
		klog(KLOG_ERROR, "Memory allocation failed for DevInst");
		if (unitAllocated) {
			ReleaseUnit(p, unit);
		}
		return 0;
	}
	di->device = dev;
	u32 x = Lock();
	TREE_ADD(tree, di, p->devTree, unit);
	/* find place where insert device in sorted list */
	DevInst *di1;
	int inserted = 0;
	LIST_FOREACH(DevInst, list, di1, p->devList) {
		if (TREE_KEY(tree, di1) < unit) {
			continue;
		}
		LIST_INSERTBEFORE(list, di, p->devList, di1);
		inserted = 1;
	}
	if (!inserted) {
		LIST_ADDLAST(list, di, p->devList);
	}
	p->numDevs++;
	Unlock(x);
	return dev;
}

Device *
DeviceManager::CreateDevice(const char *devClass, u32 unit)
{
	u32 id = GetClassID(devClass);
	return CreateDevice(id, unit);
}

Device *
DeviceManager::CreateDevice(u32 devClassID, u32 unit)
{
	u32 x = Lock();
	DevClass *p = TREE_FIND(devClassID, DevClass, node, devTree);
	Unlock(x);
	if (!p) {
		panic("Attempted to create device of non-existing class (0x%lx)", devClassID);
	}
	return CreateDevice(p, unit);
}

int
DeviceManager::DestroyDevice(Device *dev)
{
	u32 x = Lock();
	DevClass *p = TREE_FIND(dev->GetClassID(), DevClass, node, devTree);
	if (!p) {
		Unlock(x);
		panic("Destroying device of unregistered class");
	}
	u32 unit = dev->GetUnit();
	DevInst *di = TREE_FIND(unit, DevInst, tree, p->devTree);
	if (!di) {
		Unlock(x);
		panic("Destroying device with unregistered unit number (%lu)",
			unit);
	}
	LIST_DELETE(list, di, p->devList);
	TREE_DELETE(tree, di, p->devTree);
	p->numDevs--;
	DELETE(di);
	ReleaseUnit(p, unit);
	DELETE(dev);
	Unlock(x);
	return 0;
}

/* returns registered device class ID, zero if error */
u32
DeviceManager::RegisterClass(const char *devClass, Device::Type type, const char *desc,
	DeviceFactory factory, void *factoryArg)
{
	/* our constructor can be called after DeviceRegistrator constructors */
	if (isInitialized != INIT_MAGIC) {
		Initialize();
	}
	u32 id = GetClassID(devClass);
	u32 x = Lock();
	DevClass *found = TREE_FIND(id, DevClass, node, devTree);
	Unlock(x);
	if (found) {
		if (!strcmp(devClass, found->name)) {
			panic("Registering duplicated device classes: '%s'", devClass);
		}
		panic("Device class hash collision: h('%s')=h('%s')=0x%08lx",
			devClass, found->name, id);
	}
	DevClass *p = NEW(DevClass);
	if (!p) {
		panic("Device class registration failed, no memory");
	}
	klog(KLOG_DEBUG, "Registering device class '%s' (id = 0x%08lX)", devClass, id);
	p->name = strdup(devClass);
	p->desc = desc ? strdup(desc) : 0;
	p->type = type;
	p->factory = factory;
	p->factoryArg = factoryArg;
	p->firstAvailUnit = 0;
	p->numAvailUnits = ~0;
	x = Lock();
	LIST_INIT(p->devList);
	TREE_INIT(p->devTree);
	TREE_ADD(node, p, devTree, id);
	p->numDevs = 0;
	Unlock(x);
	return id;
}

u32
DeviceManager::Lock()
{
	if (im) {
		u32 x = im->DisableIntr();
		lock.Lock();
		return x;
	}
	return 0;
}

void
DeviceManager::Unlock(u32 x)
{
	if (im) {
		lock.Unlock();
		im->RestoreIntr(x);
	}
}

void
DeviceManager::ScanAvailUnits(DevClass *p)
{
	if (p->numAvailUnits) {
		return;
	}
	u32 x = Lock();
	DevInst *d = LIST_FIRST(DevInst, list, p->devList);
	p->firstAvailUnit = 0;
	while (1) {
		u32 u = TREE_KEY(tree, d);
		if (u > p->firstAvailUnit) {
			p->numAvailUnits = u - p->firstAvailUnit;
			break;
		}
		p->firstAvailUnit = u + 1;
		if (LIST_ISLAST(list, d, p->devList)) {
			p->numAvailUnits = -p->firstAvailUnit;
			break;
		}
		d = LIST_NEXT(DevInst, list, d, p->devList);
	}
	Unlock(x);
}

u32
DeviceManager::AllocateUnit(DevClass *p)
{
	ScanAvailUnits(p);
	u32 x = Lock();
	assert(p->numAvailUnits);
	p->numAvailUnits--;
	u32 unit = p->firstAvailUnit;
	p->firstAvailUnit++;
	Unlock(x);
	return unit;
}

int
DeviceManager::ReleaseUnit(DevClass *p, u32 unit)
{
	if (p->firstAvailUnit && unit == p->firstAvailUnit - 1) {
		p->firstAvailUnit--;
		p->numAvailUnits++;
	} else if (unit == p->firstAvailUnit + p->numAvailUnits) {
		p->numAvailUnits++;
	}
	return 0;
}

u32
DeviceManager::AllocateUnit(u32 devClassID)
{
	u32 x = Lock();
	DevClass *p = TREE_FIND(devClassID, DevClass, node, devTree);
	Unlock(x);
	if (!p) {
		panic("Allocating unit for non-existing device class");
	}
	return AllocateUnit(p);
}

char *
DeviceManager::GetClass(u32 devClassID)
{
	u32 x = Lock();
	DevClass *p = TREE_FIND(devClassID, DevClass, node, devTree);
	Unlock(x);
	if (!p) {
		klog(KLOG_WARNING, "Requesting class for none-existing device class ID");
		return 0;
	}
	return p->name;
}
