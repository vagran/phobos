/*
 * /kernel/dev/device.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef DEVICE_H_
#define DEVICE_H_
#include <sys.h>
phbSource("$Id$");

class DeviceManager;

class Device {
public:
	typedef struct {
		u64 addr;
		u32 size;
		u8 *buf;
	} IOBuf;

	typedef enum {
		T_CHAR,
		T_BLOCK,
		T_SPECIAL,
	} Type;

	typedef enum {
		IOS_OK,			/* operation completed successfully */
		IOS_PENDING,	/* operation queued and will be executed later */
		IOS_NODATA,		/* no data currently available (for non-blocking read) */
		IOS_NOTSPRT,	/* operation not supported by device */
		IOS_ERROR,
	} IOStatus;

private:
	Type devType;
	u32 devUnit;
	u32 devClassID;

public:
	Device(Type type, u32 unit, u32 classID);

	inline Type GetType() {return devType;}
	inline u32 GetClassID() {return devClassID;}
	inline u32 GetUnit() {return devUnit;}
};

class ChrDevice : public Device {

public:
	ChrDevice(Type type, u32 unit, u32 classID);

	virtual IOStatus Getc(u8 *c);
	virtual IOStatus Putc(u8 c);
	virtual int Read(u8 *buf, u32 size);
	virtual int Write(u8 *buf, u32 size);
};

class BlkDevice : public Device {
public:
	typedef struct {
		u64 blkIdx;
		u32 blkCount;
		u8 *data;
	} BlkIOBuf;
public:
	BlkDevice(Type type, u32 unit, u32 classID);

	virtual int Strategy(IOBuf *buf);
};

class DeviceManager {
public:
	typedef Device *(*DeviceFactory)(Device::Type type, u32 unit, u32 classID, void *arg);

	class DeviceRegistrator {
	public:
		DeviceRegistrator(const char *devClass, Device::Type type,
			DeviceFactory factory, void *factoryArg = 0);
	};
private:

	typedef struct {
		ListEntry list; /* list is sorted by unit number */
		Tree<u32>::TreeEntry tree; /* keyed by unit number */
		Device *device;
	} DevInst;

	typedef struct {
		Tree<u32>::TreeEntry node; /* keyed by ID */
		char *name;
		Device::Type type;
		DeviceFactory factory;
		void *factoryArg;
		Tree<u32>::TreeRoot devTree;
		ListHead devList;
		u32 numDevs;
		u32 firstAvailUnit;
		u32 numAvailUnits;
	} DevClass;

	enum {
		INIT_MAGIC = 0xdede1357,
	};

	Tree<u32>::TreeRoot devTree;
	SpinLock lock;
	u32 isInitialized;

	void Initialize();
	u32 AllocateUnit(DevClass *p);
	int ReleaseUnit(DevClass *p, u32 unit);
	Device *CreateDevice(DevClass *p);
	void ScanAvailUnits(DevClass *p);
	u32 inline Lock();
	void inline Unlock(u32 x);
public:
	DeviceManager();
	static inline u32 GetClassID(const char *classID)	{return gethash(classID);}
	char *GetClass(u32 devClassID);
	Device *CreateDevice(const char *devClass);
	Device *CreateDevice(u32 devClassID);
	int DestroyDevice(Device *dev);

	u32 RegisterClass(const char *devClass, Device::Type type,
		DeviceFactory factory, void *factoryArg = 0);
	u32 AllocateUnit(u32 devClassID);
};

extern DeviceManager devMan;

#define DeclareDevFactory() static Device *_DeviceFactory(Device::Type type, \
	u32 unit, u32 classID, void *arg)

#define DefineDevFactory(className) Device *className::_DeviceFactory(Device::Type type, \
	u32 unit, u32 classID, void *arg) { \
	return NEWSINGLE(className, type, unit, classID); \
}

#define _RegDevClass(devClass, type, factory, factoryArg) static DeviceManager::DeviceRegistrator \
	__UID(DeviceRegistrator)(devClass, type, factory, factoryArg)

#define RegDevClass(className, devClass, type) \
	_RegDevClass(devClass, type, className::_DeviceFactory, 0)

#endif /* DEVICE_H_ */
