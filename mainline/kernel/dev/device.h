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
		T_BLOCK
	} Type;

	typedef enum {
		IOS_OK,			/* operation completed successfully */
		IOS_PENDING,	/* operation queued and will be executed later */
		IOS_NODATA,		/* no data currently available (for non-blocking read) */
		IOS_NOTSPRT,	/* operation not supported by device */
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
	virtual int Strategy(IOBuf *buf);
};

class DeviceManager {
public:
	typedef Device *(*DeviceFactory)(Device::Type type, u32 unit, u32 classID, void *arg);

private:
	typedef struct {
		TreeEntry node;
		char *name;
		u32 id;
		Device::Type type;
		DeviceFactory factory;
		void *factoryArg;
		int availUnit;
	}DevClass;

	enum {
		INIT_MAGIC = 0xdede1357,
	};

	TreeRoot devTree;
	u32 isInitialized;

	void Initialize();
	u32 AllocateUnit(DevClass *p);
	Device *CreateDevice(DevClass *p);
public:
	DeviceManager();
	static inline u32 GetClassID(char *classID)	{return gethash(classID);}
	char *GetClass(u32 devClassID);
	Device *CreateDevice(char *devClass);
	Device *CreateDevice(u32 devClassID);

	u32 RegisterClass(char *devClass, Device::Type type, DeviceFactory factory, void *factoryArg = 0);
	u32 AllocateUnit(u32 devClassID);
};

extern DeviceManager devMan;

class DeviceRegistrator {
public:
	DeviceRegistrator(char *devClass, Device::Type type,
		DeviceManager::DeviceFactory factory, void *factoryArg = 0);
};

#define RegDevClass(devClass, type, factory, factoryArg) static DeviceRegistrator \
	__UID(DeviceRegistrator)(devClass, type, factory, factoryArg)

#endif /* DEVICE_H_ */
