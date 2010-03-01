/*
 * /phobos/kernel/vfs/vfs.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef VFS_H_
#define VFS_H_
#include <sys.h>
phbSource("$Id$");

class VFS {
public:
	class Mount;

	class Node {
	public:
		enum Type {
			T_NONE,
			T_REGULAR,
			T_DIRECTORY,
			T_CHARDEV,
			T_BLOCKDEV,
			T_PIPE,
			T_SOCKET,
			T_LINK,
			T_MAX
		};
	private:
		friend class VFS;

		ListEntry list; /* list in parent node */
		ListHead childs;
		Tree<u32>::TreeRoot hashTree;
		Tree<u32>::TreeEntry hashTreeEntry;
		ListHead hashList;
		ListEntry hashListEntry;
		u32 hash;
		int hashHead;
		Node *parent;
		u32 numChilds;
		Type type;
		char *name;
		Mount *mount;
		Node *prevMount; /* the node which was replaced if mounted on this node */
		Handle fsNode;
		AtomicInt<u32> refCount;

	public:
		Node(Type type, Node *parent, const char *name, int nameLen = -1);
		~Node();
		void AddRef();
		int Release();
	};

	class Mount {
	private:
		RefCount refCount;
		DeviceFS *fs;
		BlkDevice *dev;
	public:
		Mount(BlkDevice *dev, int flags, const char *fsType = 0);
		~Mount();
		OBJ_ADDREF(refCount);
		OBJ_RELEASE(refCount);
		inline DeviceFS *GetFS() { return fs; }
	};

	class File {
	private:
		RefCount refCount;
	public:
		File();
		~File();
		OBJ_ADDREF(refCount);
		OBJ_RELEASE(refCount);

	};

private:
	Node *root;
	SpinLock treeLock;

	Node *AllocateNode(Node *parent, Node::Type type, const char *name, int nameLen = -1);
	int DeallocateNode(Node *node);
	Mount *CreateMount(BlkDevice *dev, int flags, const char *fsType = 0);
	Node *LookupNode(const char *path);
	Node *GetSubNode(Node *parent, const char *name, int nameLen = -1);
	Node *GetNode(Node *parent, const char *name, int nameLen = -1);
public:
	VFS();

	int MountDevice(BlkDevice *dev, const char *mountPoint, int flags = 0,
		const char *type = 0);
	File *CreateFile(const char *path);
};

extern VFS *vfs;

#endif /* VFS_H_ */
