/*
 * /phobos/kernel/vfs/vfs.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

VFS *vfs;

VFS::VFS()
{
	TREE_INIT(nodes);
	root = AllocateNode(0, 0, Node::T_DIRECTORY, ".");
	ensure(root);
}

VFS::Node *
VFS::AllocateNode(Node *parent, nodeid_t id, Node::Type type, const char *name)
{
	Node *node = NEW(Node, type, parent, name);
	if (!node) {
		return 0;
	}
	treeLock.Lock();
	TREE_ADD(tree, node, nodes, id);
	if (parent) {
		LIST_ADD(list, node, parent->childs);
		parent->numChilds++;
	}
	treeLock.Unlock();
	return node;
}

int
VFS::DeallocateNode(Node *node)
{
	/* XXX */
	DELETE(node);
	return 0;
}

int
VFS::Mount(BlkDevice *dev, const char *mountPoint)
{
	assert(dev->GetType() == Device::T_BLOCK);
	//notimpl
	return 0;
}

/******************************************************/
/* VFS::Node class */

VFS::Node::Node(Type type, Node *parent, const char *name)
{
	LIST_INIT(childs);
	this->type = type;
	this->parent = parent;
	this->name = strdup(name);
	if (parent) {
		mount = parent->mount;
	} else {
		mount = 0;
	}
	prevMount = 0;
}

VFS::Node::~Node()
{
	if (name) {
		MM::mfree(name);
	}
}

/******************************************************/
/* VFS::Mount class */

VFS::Mount::Mount()
{

}

VFS::Mount::~Mount()
{

}
