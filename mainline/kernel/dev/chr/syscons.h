/*
 * /kernel/dev/chr/syscons.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef SYSCONS_H_
#define SYSCONS_H_
#include <sys.h>
phbSource("$Id$");

#include <dev/condev.h>

class SysConsole : public ConsoleDev {
public:
	class SysSerial : public ChrDevice {
	private:
		u16 iobase;
		u16 divisor;
		SpinLock lock;

		void Initialize();
		void SetSpeed(int speed);
	public:
		DeclareDevFactory();
		SysSerial(Type type, u32 unit, u32 classID);
		virtual IOStatus Getc(u8 *c);
		virtual IOStatus Putc(u8 c);
	};
private:
	ChrDevice *defInput; /* default input device */
	ChrDevice *defOutput; /* default output device */

	typedef struct {
		ListEntry list;
		ChrDevice *dev;
	} OutputClient;

	ListHead outClients;
	Mutex outClientsMtx;
public:
	DeclareDevFactory();
	SysConsole(Type type, u32 unit, u32 classID);
	virtual ~SysConsole();

	/* ConsoleDev implementation */
	virtual IOStatus Putc(u8 c);

	/* Specific extension */
	virtual int RestoreDefInputDevice();
	virtual int AddOutputDevice(ChrDevice *dev);
	virtual int RemoveOutputDevice(ChrDevice *dev);
};

extern SysConsole *sysCons;

#endif /* SYSCONS_H_ */
