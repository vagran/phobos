/*
 * /kernel/dev/condev.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef CONDEV_H_
#define CONDEV_H_
#include <sys.h>
phbSource("$Id$");

class ConsoleDev;

#include <dev/device.h>
#include <kern/im.h>

class ConsoleDev : public ChrDevice {
public:
	enum {
		DEF_QUEUE_SIZE =	16384,
	};

	typedef enum {
		K_BACKSPACE =		8,
		K_UP =				65,
		K_DOWN =			66,
		K_RIGHT =			67,
		K_LEFT =			68,
	} Keys;

	typedef enum {
		COL_BLACK,
		COL_RED,
		COL_GREEN,
		COL_YELLOW,
		COL_BLUE,
		COL_MAGENTA,
		COL_CYAN,
		COL_WHITE,
		COL_BRIGHT = 0x10,
	} Colors;

private:
	ChrDevice *inDev;
	typedef struct {
		ListEntry list;
		ChrDevice *dev;
	} OutputClient;

	typedef struct {
		u32 size;
		u8 *data;
		u32 pRead, pWrite;
		u32 dataSize;
		SpinLock lock;
	} IOQueue;

	ListHead outClients;
	SpinLock outClientsMtx;
	IOQueue outQueue;
	Handle outIrq;

	static void _Putc(int c, ConsoleDev *p);
	IM::IsrStatus OutputIntr(Handle h);
	Device::IOStatus QPutc(u8 c);
	Device::IOStatus DevPutc(u8 c);
protected:
	int fgCol, bgCol;
	int tabSize;

public:
	ConsoleDev(Type type, u32 unit, u32 classID);
	virtual ~ConsoleDev();

	virtual IOStatus Getc(u8 *c);
	virtual IOStatus Putc(u8 c);
	virtual int SetFgColor(int col); /* return previous value */
	virtual int SetBgColor(int col); /* return previous value */
	virtual int Clear();
	virtual int SetTabSize(int sz); /* return previous value */

	virtual int VPrintf(const char *fmt, va_list va) __format(printf, 2, 0);
	virtual int Printf(const char *fmt,...) __format(printf, 2, 3);

	virtual int SetInputDevice(ChrDevice *dev);
	virtual int AddOutputDevice(ChrDevice *dev);
	virtual int RemoveOutputDevice(ChrDevice *dev);
	virtual int SetQueue(u32 inputQueueSize = DEF_QUEUE_SIZE,
		u32 outputQueueSize = DEF_QUEUE_SIZE); /* zero to disable queuing */
};

#endif /* CONDEV_H_ */
