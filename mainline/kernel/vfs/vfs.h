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
	typedef u64 nodeid_t;

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

		Tree<nodeid_t>::TreeEntry tree;
		ListEntry list; /* list in parent node */
		ListHead childs;
		Node *parent;
		u32 numChilds;
		Type type;
		char *name;
		Mount *mount;
		Node *prevMount; /* the mode which was replaced if mounted on this node */

	public:
		Node(Type type, Node *parent, const char *name);
		~Node();

		inline nodeid_t GetID() { return TREE_KEY(tree, this); }
	};

	class Mount {
	private:

	public:
		Mount();
		~Mount();
	};

private:
	Node *root;
	Tree<nodeid_t>::TreeRoot nodes;
	SpinLock treeLock;

	Node *AllocateNode(Node *parent, nodeid_t id, Node::Type type, const char *name);
	int DeallocateNode(Node *node);
public:
	VFS();

	int Mount(BlkDevice *dev, const char *mountPoint);
};

extern VFS *vfs;

#endif /* VFS_H_ */
