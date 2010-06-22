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

extern DeviceManager devMan;

class Device {
public:
	class IOBuf {
	public:
		enum Flags {
			F_DIR =			0x1,
			F_DIR_IN = 		0x0,
			F_DIR_OUT =		0x1,
			F_COMPLETE =	0x2,
			F_QUEUED =		0x4, /* queued by device driver */
			F_OWNBUF =		0x8, /* data buffer allocated and freed by IOBuf */
		};

		enum Status {
			S_ABORTED =		-32767,
			S_OUT_OF_RANGE,			/* buffer addressed inaccessible location */
			S_CUSTOM_ERROR,			/* custom device error codes can start from this value */

			S_OK =			0,
			S_PENDING,

			S_CUSTOM_STATUS,		/* custom device status codes can start from this value */
		};

		ListEntry list; /* for device queue */
		u16 flags;
		i16 status; /* zero if successfully completed */
		u64 addr;
		u32 size;
		void *buf;
		Device *dev;
		RefCount refCount;

		IOBuf(u32 size);
		~IOBuf();

		/* Data buffer is allocated when size is non-zero */
		static IOBuf *AllocateBuffer(u32 size = 0);
		OBJ_ADDREF(refCount);
		OBJ_RELEASE(refCount);
		void Setup(u64 addr, int direction, u32 size = 0, void *buf = 0);
		int Wait(u64 timeout = 0);
		inline int GetDirection() { return flags & F_DIR; }
	};

	typedef enum {
		T_CHAR,
		T_BLOCK,
		T_SPECIAL,
	} Type;

	typedef enum {
		S_DOWN,
		S_UP,
		S_NOTFOUND,
	} State;

	typedef enum {
		IOS_OK,			/* operation completed successfully */
		IOS_PENDING,	/* operation queued and will be executed later */
		IOS_NODATA,		/* no data currently available (for non-blocking read) */
		IOS_NOTSPRT,	/* operation not supported by device */
		IOS_ERROR,
	} IOStatus;

protected:
	Type devType;
	u32 devUnit;
	u32 devClassID;
	State devState;
	u32 refCount;
	u32 blockSize;

	int AcceptBuffer(IOBuf *buf, int queue = 0);
	int ReleaseBuffer(IOBuf *buf);
	int CompleteBuffer(IOBuf *buf, int status = IOBuf::S_OK);
public:
	Device(Type type, u32 unit, u32 classID);
	virtual ~Device();

	inline Type GetType() {return devType;}
	inline u32 GetClassID() {return devClassID;}
	inline u32 GetUnit() {return devUnit;}
	inline State GetState() {return devState;}
	inline char *GetClass();

	virtual int AddRef();
	virtual int Release();

	inline u32 GetBlockSize() { return blockSize; }

	static IOBuf *AllocateBuffer();
};

class ChrDevice : public Device {

public:
	ChrDevice(Type type, u32 unit, u32 classID);
	virtual ~ChrDevice();

	virtual IOStatus Getc(u8 *c);
	virtual IOStatus Putc(u8 c);
	virtual int Read(u8 *buf, u32 size);
	virtual int Write(u8 *buf, u32 size);
};

class BlkDevice : public Device {
public:
	enum {
		DEF_BLOCK_SIZE =	512,
	};
protected:
	u64 size; /* in blocks */
public:
	BlkDevice(Type type, u32 unit, u32 classID);
	virtual ~BlkDevice();

	inline u64 GetSize() { return size * blockSize; }

	virtual int Push(IOBuf *buf) = 0;
	virtual int Pull(IOBuf *buf) = 0;

	int Read(u64 addr, void *buf, u32 size);
};

class DeviceManager {
public:
	typedef Device *(*DeviceFactory)(Device::Type type, u32 unit, u32 classID, void *arg);
	typedef int (*DeviceProber)(int lastUnit, void *arg); /* return next found unit, -1 if none */

	class DeviceRegistrator {
	public:
		DeviceRegistrator(const char *devClass, Device::Type type, const char *desc,
			DeviceFactory factory, DeviceProber prober = 0, void *arg = 0);
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
		char *desc;
		Device::Type type;
		DeviceFactory factory;
		DeviceProber prober;
		void *arg;
		Tree<u32>::TreeRoot devTree;
		ListHead devList;
		u32 numDevs;
		u32 firstAvailUnit;
		u32 numAvailUnits;
	} DevClass;

	enum {
		INIT_MAGIC = 0xdede1357,
		DEF_UNIT = 0xffffffff,
	};

	Tree<u32>::TreeRoot devTree;
	SpinLock lock;
	u32 isInitialized;

	void Initialize();
	u32 AllocateUnit(DevClass *p);
	DevInst *FindDevice(DevClass *p, u32 unit);
	inline DevClass *FindClass(u32 id);
	int ReleaseUnit(DevClass *p, u32 unit);
	Device *CreateDevice(DevClass *p, u32 unit = DEF_UNIT);
	void ScanAvailUnits(DevClass *p);
	u32 inline Lock();
	void inline Unlock(u32 x);
public:
	DeviceManager();
	static inline u32 GetClassID(const char *classID)	{ return gethash(classID); }
	char *GetClass(u32 devClassID);
	Device *CreateDevice(const char *devClass, u32 unit = DEF_UNIT);
	Device *CreateDevice(u32 devClassID, u32 unit = DEF_UNIT);
	int DestroyDevice(Device *dev);
	Device *GetDevice(const char *devClass, u32 unit);
	Device *GetDevice(u32 devClassID, u32 unit);
	Device *GetDevice(const char *devName);
	int ProbeDevices();

	u32 RegisterClass(const char *devClass, Device::Type type, const char *desc,
		DeviceFactory factory, DeviceProber prober = 0, void *arg = 0);
	u32 AllocateUnit(u32 devClassID);
};

char *Device::GetClass() { return devMan.GetClass(GetClassID()); }

#define DeclareDevFactory() static Device *_DeviceFactory(Device::Type type, \
	u32 unit, u32 classID, void *arg)

#define DefineDevFactory(className) Device *className::_DeviceFactory(Device::Type type, \
	u32 unit, u32 classID, void *arg) { \
	return NEWSINGLE(className, type, unit, classID); \
}

#define DeclareDevProber() static int _DeviceProber(int lastUnit = -1, void *arg = 0)

#define DefineDevProber(className) int className::_DeviceProber(int lastUnit, void *arg)

#define RegDevClass(className, devClass, type, desc,...) static DeviceManager::DeviceRegistrator \
	__UID(DeviceRegistrator)(devClass, type, desc, className::_DeviceFactory, 0, ## __VA_ARGS__)

#define RegProbeDevClass(className, devClass, type, desc,...) static DeviceManager::DeviceRegistrator \
	__UID(DeviceRegistrator)(devClass, type, desc, className::_DeviceFactory, \
		className::_DeviceProber, ## __VA_ARGS__)

#endif /* DEVICE_H_ */
