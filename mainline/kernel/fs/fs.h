/*
 * /phobos/kernel/fs/fs.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
 */

#ifndef FS_H_
#define FS_H_
#include <sys.h>
phbSource("$Id$");

/* Device filesystem */

class DeviceFS;

#include <vfs/vfs.h>

class DeviceFS : public Object {
public:
	enum Flags {
		F_READONLY =		0x1,
	};
	typedef DeviceFS *(*Factory)(BlkDevice *dev, int flags, void *arg);
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

	static FSEntry *GetFS(const char *name);
protected:
	BlkDevice *dev;
	int status;
	int flags;

public:
	DeviceFS(BlkDevice *dev, int flags);
	virtual ~DeviceFS();

	static DeviceFS *Create(BlkDevice *dev, int flags, const char *name);
	inline int GetStatus() { return status; }
	/*
	 * 'parent' is zero for entries in root directory, 'parent' and 'name'
	 * are zero for root directory.
	 */
	virtual Handle GetNode(Handle parent, const char *name, int nameLen = -1) = 0;
	virtual VFS::Node::Type GetNodeType(Handle node) = 0;
	virtual u64 GetNodeSize(Handle node) = 0;
	virtual i64 ReadNode(Handle node, off_t offset, u32 len, void *buf) = 0;
	virtual u32 ReadLink(Handle node, void *buf, u32 bufLen) = 0;
};

#define DeclareFSFactory() static DeviceFS *_FSFactory(BlkDevice *dev, int flags, void *arg)

#define DefineFSFactory(className) DeviceFS * \
className::_FSFactory(BlkDevice *dev, int flags, void *arg) \
{ \
	DeviceFS *fs = NEWSINGLE(className, dev, flags); \
	if (!fs) { \
		return 0; \
	} \
	if (fs->GetStatus()) { \
		DELETE(fs); \
		return 0; \
	} \
	return fs; \
}

#define DeclareFSProber() static int _FSProber(BlkDevice *dev, void *arg)

#define DefineFSProber(className) int className::_FSProber(BlkDevice *dev, void *arg)

#define RegisterFS(className, fsId, desc,...) static DeviceFS::FSRegistrator \
	__UID(FSRegistrator)(fsId, desc, className::_FSFactory, className::_FSProber, ## __VA_ARGS__)

#endif /* FS_H_ */
