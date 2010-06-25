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

class VFS : public Object {
public:
	class Mount;

	class Node : public Object {
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

		inline Type GetType() { return type; }
		u32 GetSize();
		u32 Read(u64 offset, void *buf, u32 len);
	};

	class Mount : public Object {
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

	class File : public Object {
	protected:
		RefCount refCount;
		Node *node;
	public:
		File(Node *node);
		virtual ~File();
		OBJ_ADDREF(refCount);
		OBJ_RELEASE(refCount);

		inline Node::Type GetType() { return node->GetType(); }

		virtual u32 Read(u64 offset, void *buf, u32 len);
		virtual u32 GetSize();
	};

	class Directory : public File {
	private:
	public:
		Directory(Node *node);
		virtual ~Directory();

		virtual u32 Read(u64 offset, u32 len, void *buf);
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
	MM::VMObject *MapFile(const char *path);
	MM::VMObject *MapFile(File *file);
};

extern VFS *vfs;

#endif /* VFS_H_ */
