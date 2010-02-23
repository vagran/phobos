/*
 * /phobos/kernel/fs/fs.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef FS_H_
#define FS_H_
#include <sys.h>
phbSource("$Id$");

/* Device filesystem */

class DeviceFS {
public:
	typedef DeviceFS *(*Factory)(BlkDevice *dev, void *arg);
	typedef int (*Prober)(BlkDevice *dev, void *arg); /* must return 0 if filesystem recognized */

	class FSRegistrator {
	public:
		FSRegistrator(const char *id, const char *desc, Factory factory, Prober prober, void *arg = 0);
	};
	static int RegisterFilesystem(const char *id, const char *desc, Factory factory, Prober prober, void *arg);
private:
	typedef struct {
		ListEntry list;
		char *id;
		char *desc;
		Factory factory;
		Prober prober;
		void *arg;
	} FSEntry;
	static ListHead fsReg;
	static u32 numFsReg;
protected:
	BlkDevice *dev;

public:
	DeviceFS(BlkDevice *dev);
	virtual ~DeviceFS();
};

#define DeclareFSFactory() static DeviceFS *_FSFactory(BlkDevice *dev, void *arg)

#define DefineFSFactory(className) DeviceFS * \
className::_FSFactory(BlkDevice *dev, void *arg) \
{ \
	return NEWSINGLE(className, dev); \
}

#define DeclareFSProber() static int _FSProber(BlkDevice *dev, void *arg)

#define DefineFSProber(className) int className::_FSProber(BlkDevice *dev, void *arg)

#define RegisterFS(className, fsId, desc,...) static DeviceFS::FSRegistrator \
	__UID(FSRegistrator)(fsId, desc, className::_FSFactory, className::_FSProber, ## __VA_ARGS__)

#endif /* FS_H_ */
