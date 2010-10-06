/*
 * /kernel/dev/device.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/chr/KbdDev.h>
#include <dev/chr/KbdConsole.h>

Device::Device(Type type, u32 unit, u32 classID)
{
	refCount = 1;
	devType = type;
	devUnit = unit;
	devClassID = classID;
	blockSize = 1;
	devState = S_DOWN;
}

Device::~Device()
{

}

int
Device::AddRef()
{
	return ++refCount;
}

int
Device::Release()
{
	assert(refCount);
	return --refCount;
}

int
Device::AcceptBuffer(IOBuf *buf, int queue)
{
	ensure(buf->size && buf->buf);
	assert(!((buf->size | buf->addr) & (blockSize - 1)));
	buf->flags &= IOBuf::F_COMPLETE;
	buf->status = 0;
	buf->dev = this;
	pm->ReserveSleepChannel(buf);

	/* XXX queuing not implemented yet */
	return 0;
}

int
Device::ReleaseBuffer(IOBuf *buf)
{
	/* XXX queuing not implemented yet */
	assert(buf->flags & IOBuf::F_COMPLETE);
	buf->dev = 0;
	pm->FreeSleepChannel(buf);
	return 0;
}

int
Device::CompleteBuffer(IOBuf *buf, int status)
{
	/* XXX queuing not implemented yet */
	buf->flags = IOBuf::F_COMPLETE;
	buf->status = status;
	return 0;
}

Device::IOBuf *
Device::AllocateBuffer()
{
	return IOBuf::AllocateBuffer();
}

/* Device::IOBuf class */

Device::IOBuf::IOBuf(u32 size)
{
	if (size) {
		flags = F_OWNBUF;
		buf = MM::malloc(size);
		ensure(buf);
	} else {
		flags = 0;
		buf = 0;
	}
	status = 0;
	addr = 0;
	this->size = size;
	dev = 0;
}

Device::IOBuf::~IOBuf()
{
	if (flags & F_OWNBUF) {
		if (buf) {
			MM::mfree(buf);
		}
	}
}

Device::IOBuf *
Device::IOBuf::AllocateBuffer(u32 size)
{
	IOBuf *buf = NEW(IOBuf, size);
	return buf;
}

int
Device::IOBuf::Wait(u64 timeout)
{
	int rc;
	void *wakenBy;

	while (!(flags & F_COMPLETE)) {
		rc = pm->Sleep(this, "IOBuf::Wait", timeout, &wakenBy);
		if (rc) {
			break;
		}
		if (!wakenBy) {
			rc = 1;
			break;
		}
	}
	return rc;
}

void
Device::IOBuf::Setup(u64 addr, int direction, u32 size, void *buf)
{
	this->addr = addr;
	assert(!(direction & ~F_DIR));
	if (direction == F_DIR_IN) {
		flags |= F_DIR_IN;
	} else {
		flags |= F_DIR_OUT;
	}
	status = S_PENDING;
	if (size) {
		assert(!(flags & F_OWNBUF) || size <= this->size);
		this->size = size;
	}
	if (buf) {
		assert(!(flags & F_OWNBUF));
		this->buf = buf;
	}
}

/**************************************************************
 * Character device
 */

ChrDevice::ChrDevice(Type type, u32 unit, u32 classID) :
	Device(type, unit, classID)
{
	memset(evCbk, 0, sizeof(evCbk));
}

ChrDevice::~ChrDevice()
{

}

int
ChrDevice::Notify(Operation op)
{
	if (op >= OP_MAX) {
		return -1;
	}
	if (evCbk[op].obj) {
		(evCbk[op].obj->*evCbk[op].cbk)(this, op);
	}
	u32 waitId = GetWaitChannel(op);
	if (waitId) {
		pm->Wakeup((PM::waitid_t)waitId);
	}
	return 0;
}

int
ChrDevice::RegisterCallback(Operation op, EventCallback cbk, Object *obj)
{
	if (op >= OP_MAX) {
		return -1;
	}
	evCbk[op].obj = obj;
	evCbk[op].cbk = cbk;
	return 0;
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
	blockSize = DEF_BLOCK_SIZE;
	size = 0;
}

BlkDevice::~BlkDevice()
{

}

int
BlkDevice::Read(u64 addr, void *buf, u32 size)
{

	/* XXX should be able to read from unaligned location */
	assert(!(addr & (blockSize - 1)));
	IOBuf *iob1 = AllocateBuffer(), *iob2 = 0;
	if (!iob1) {
		return -1;
	}
	u8 *buf1 = (u8 *)MM::malloc(blockSize);
	assert(buf1);
	iob1->Setup(addr, IOBuf::F_DIR_IN);
	if (size < blockSize) {
		iob1->size = blockSize;
		iob1->buf = buf1;
	} else {
		iob1->size = rounddown(size, blockSize);
		iob1->buf = buf;
		if (iob1->size != size) {
			iob2 = AllocateBuffer();
			if (!iob2) {
				iob1->Release();
				MM::mfree(buf1);
				return -1;
			}
			iob2->Setup(iob2->addr, IOBuf::F_DIR_IN, blockSize, buf1);
		}
	}
	if (Push(iob1)) {
		iob1->Release();
		if (iob2) {
			iob2->Release();
		}
		MM::mfree(buf1);
		return -1;
	}
	if (iob2) {
		if (Push(iob2)) {
			iob2->Release();
			iob1->Wait();
			Pull(iob1);
			iob1->Release();
			MM::mfree(buf1);
			return -1;
		}
	}

	iob1->Wait();
	Pull(iob1);
	if (iob2) {
		iob2->Wait();
		Pull(iob2);
	}
	int rc = iob1->status == IOBuf::S_OK ? 0 : -1;
	if (iob2) {
		rc |= iob2->status == IOBuf::S_OK ? 0 : -1;
	}
	if (!rc) {
		if (size < blockSize) {
			memcpy(buf, buf1, size);
		} else if (iob2) {
			memcpy((u8 *)buf + iob1->size, buf1, size - iob1->size);
		}
	}
	iob1->Release();
	if (iob2) {
		iob2->Release();
	}
	MM::mfree(buf1);
	return rc;
}

/**************************************************************
 * Device manager class implementation
 */

/* system global device manager instance */
DeviceManager devMan;

DeviceManager::DeviceRegistrator::DeviceRegistrator(const char *devClass, Device::Type type,
	const char *desc, DeviceFactory factory, DeviceProber prober, void *arg)
{
	devMan.RegisterClass(devClass, type, desc, factory, prober, arg);
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
	Device *dev = p->factory(p->type, unit, TREE_KEY(node, p), p->arg);
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
		LIST_INSERT_BEFORE(list, di, p->devList, di1);
		inserted = 1;
	}
	if (!inserted) {
		LIST_ADD_LAST(list, di, p->devList);
	}
	p->numDevs++;
	Unlock(x);
	klog(KLOG_INFO, "Device created: %s%lu - %s", p->name, unit, p->desc);
	return dev;
}

Device *
DeviceManager::CreateDevice(const char *devClass, u32 unit)
{
	u32 id = GetClassID(devClass);
	return CreateDevice(id, unit);
}

Device *
DeviceManager::GetDevice(const char *devClass, u32 unit)
{
	u32 id = GetClassID(devClass);
	return GetDevice(id, unit);
}

Device *
DeviceManager::GetDevice(const char *devName)
{
	/* find unit number */
	u32 unitPos = strlen(devName) - 1;
	if (!isdigit(devName[unitPos])) {
		return 0;
	}
	while (unitPos && isdigit(devName[unitPos - 1])) {
		unitPos--;
	}
	char devClass[256];
	int n = min(unitPos, sizeof(devClass) - 1);
	memcpy(devClass, devName, n);
	devClass[n] = 0;
	u32 unit = strtoul(&devName[unitPos], 0, 10);
	return GetDevice(devClass, unit);
}

Device *
DeviceManager::GetDevice(u32 devClassID, u32 unit)
{
	DevClass *p = FindClass(devClassID);
	if (!p) {
		return 0;
	}
	DevInst *dev = FindDevice(p, unit);
	if (!dev) {
		return 0;
	}
	return dev->device;
}

int
DeviceManager::ProbeDevices()
{
	DevClass *p;
	TREE_FOREACH(DevClass, node, p, devTree) {
		if (!p->prober) {
			continue;
		}
		int unit = -1;
		while ((unit = p->prober(unit, p->arg)) != -1) {
			if (FindDevice(p, unit)) {
				continue;
			}
			Device *d = CreateDevice(p, unit);
			if (!d) {
				break;
			}
		}
	}
	return 0;
}

int
DeviceManager::ConfigureDevices()
{
	ProbeDevices();

	/* Check for old keyboard controller */
	KbdDev *kbd = (KbdDev *)GetDevice("8042kbd0");

	/* Create keyboard console if we have keyboard */
	if (kbd) {
		KbdConsole *kbdCons = (KbdConsole *)CreateDevice("kbdcons");
		if (kbdCons) {
			kbdCons->SetInputDevice(kbd);
		}
	}
	return 0;
}

DeviceManager::DevClass *
DeviceManager::FindClass(u32 id)
{
	u32 x = Lock();
	DevClass *p = TREE_FIND(id, DevClass, node, devTree);
	Unlock(x);
	return p;
}

Device *
DeviceManager::CreateDevice(u32 devClassID, u32 unit)
{
	DevClass *p = FindClass(devClassID);
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
	DeviceFactory factory, DeviceProber prober, void *arg)
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
	p->prober = prober;
	p->arg = arg;
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
		u32 x = IM::DisableIntr();
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
		IM::RestoreIntr(x);
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
		d = LIST_NEXT(DevInst, list, d);
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
