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
	root = AllocateNode(0, Node::T_DIRECTORY, ".");
	ensure(root);
}

VFS::Node *
VFS::AllocateNode(Node *parent, Node::Type type, const char *name, int nameLen)
{
	Node *node = NEW(Node, type, parent, name, nameLen);
	if (!node) {
		return 0;
	}
	treeLock.Lock();
	if (parent) {
		LIST_ADD(list, node, parent->childs);
		parent->numChilds++;
		Node *hashNode = TREE_FIND(node->hash, Node, hashTreeEntry, parent->hashTree);
		if (!hashNode) {
			TREE_ADD(hashTreeEntry, node, parent->hashTree, node->hash);
			node->hashHead = 1;
		} else {
			LIST_ADD(hashListEntry, node, hashNode->hashList);
			node->hashHead = 0;
		}
	}
	treeLock.Unlock();
	return node;
}

int
VFS::DeallocateNode(Node *node)
{
	assert(!node->numChilds);
	treeLock.Lock();
	if (node->parent) {
		assert(node->parent->numChilds);
		node->parent->numChilds--;
		LIST_DELETE(list, node, node->parent->childs);
		if (node->hashHead) {
			Node *hashNode = LIST_FIRST(Node, hashListEntry, node->hashList);
			if (hashNode) {
				LIST_DELETE(hashListEntry, hashNode, node->hashList);
				hashNode->hashHead = 1;
				hashNode->hashList = node->hashList;
				TREE_DELETE(hashTreeEntry, node, node->parent->hashTree);
				TREE_ADD(hashTreeEntry, hashNode, node->parent->hashTree, hashNode->hash);
			} else {
				TREE_DELETE(hashTreeEntry, node, node->parent->hashTree);
			}
		} else {
			Node *hashHead = TREE_FIND(node->hash, Node, hashTreeEntry, node->parent->hashTree);
			assert(hashHead);
			LIST_DELETE(hashListEntry, node, hashHead->hashList);
		}
	}
	treeLock.Unlock();
	DELETE(node);
	return 0;
}

VFS::Node *
VFS::GetNode(Node *parent, const char *name, int nameLen)
{
	if (!parent->numChilds) {
		return 0;
	}
	u32 hash = nameLen == -1 ? gethash(name) : gethash((u8 *)name, nameLen);
	Node *node;
	if (parent->numChilds < 32) {
		LIST_FOREACH(Node, list, node, parent->childs) {
			if (node->hash != hash) {
				continue;
			}
			if (nameLen == -1) {
				if (!strncmp(name, node->name, nameLen) && !node->name[nameLen]) {
					return node;
				}
			} else {
				if (!strcmp(name, node->name)) {
					return node;
				}
			}
		}
		return 0;
	}
	node = TREE_FIND(hash, Node, hashTreeEntry, parent->hashTree);
	if (!node) {
		return 0;
	}
	if (nameLen == -1) {
		if (!strncmp(name, node->name, nameLen) && !node->name[nameLen]) {
			return node;
		}
	} else {
		if (!strcmp(name, node->name)) {
			return node;
		}
	}
	Node *hashHead = node;
	LIST_FOREACH(Node, hashListEntry, node, hashHead->hashList) {
		if (nameLen == -1) {
			if (!strncmp(name, node->name, nameLen) && !node->name[nameLen]) {
				return node;
			}
		} else {
			if (!strcmp(name, node->name)) {
				return node;
			}
		}
	}
	return 0;
}

VFS::Node *
VFS::GetSubNode(Node *parent, const char *name, int nameLen)
{
	Node *node = GetNode(parent, name, nameLen);
	if (node) {
		return node;
	}
	/* no node in cache, query underlying file system */
	if (!parent->mount) {
		return 0;
	}
	DeviceFS *fs = parent->mount->GetFS();
	Handle fsNode = fs->GetNode(parent->fsNode, name, nameLen);
	if (!fsNode) {
		return 0;
	}
	/* create new entry in cache */
	Node::Type type = fs->GetNodeType(fsNode);
	node = AllocateNode(parent, type, name, nameLen);
	if (!node) {
		return 0;
	}
	node->fsNode = fsNode;
	return node;
}

VFS::Node *
VFS::LookupNode(const char *path)
{
	const char *cp, *next = path;
	Node *node = root;
	treeLock.Lock();
	while (*next && node) {
		for (cp = next; *cp && *cp == '/'; cp++);
		if (!*cp) {
			break;
		}
		for (next = cp; *next && *next != '/'; next++);
		node = GetSubNode(node, cp, next - cp);
	}
	treeLock.Unlock();
	return node;
}

VFS::Mount *
VFS::CreateMount(BlkDevice *dev, const char *fsType)
{
	Mount *p = NEW(Mount, dev, fsType);
	if (!p) {
		return 0;
	}
	if (!p->GetFS()) {
		p->Release();
		return 0;
	}
	return p;
}

int
VFS::MountDevice(BlkDevice *dev, const char *mountPoint, const char *type)
{
	assert(dev->GetType() == Device::T_BLOCK);
	Mount *m = CreateMount(dev, type);
	if (!m) {
		return -1;
	}
	Node *node = LookupNode(mountPoint);
	if (!node) {
		m->Release();
		klog(KLOG_WARNING, "Mount point not found: '%s'", mountPoint);
		return 0;
	}
	node->AddRef();
	/* create new node, and replace old node with new one */
	Node *mNode = AllocateNode(node->parent, Node::T_DIRECTORY, node->name);
	treeLock.Lock();
	if (node->parent) {
		LIST_DELETE(list, node, node->parent->childs);
		node->parent->numChilds--;
	}
	mNode->prevMount = node;
	mNode->mount = m;
	mNode->fsNode = m->GetFS()->GetNode(0, 0);
	ensure(mNode->fsNode);
	treeLock.Unlock();
	return 0;
}

VFS::File *
VFS::CreateFile(const char *path)
{
	Node *node = LookupNode(path);
	if (!node) {
		return 0;
	}
	//notimpl
	return 0;
}

/******************************************************/
/* VFS::Node class */

VFS::Node::Node(Type type, Node *parent, const char *name, int nameLen)
{
	LIST_INIT(childs);
	numChilds = 0;
	TREE_INIT(hashTree);
	LIST_INIT(hashList);
	this->type = type;
	this->parent = parent;
	if (nameLen == -1) {
		this->name = strdup(name);
	} else {
		this->name = (char *)MM::malloc(nameLen + 1, 1);
		memcpy(this->name, name, nameLen);
		this->name[nameLen] = 0;
	}
	if (parent) {
		mount = parent->mount;
	} else {
		mount = 0;
	}
	prevMount = 0;
	fsNode = 0;
	refCount = 0;
	hashHead = 0;
	hash = gethash(this->name);
}

VFS::Node::~Node()
{
	assert(!refCount);
	if (name) {
		MM::mfree(name);
	}
}

void
VFS::Node::AddRef()
{
	for (Node *node = this; node; node = node->parent) {
		++node->refCount;
	}
}

int
VFS::Node::Release()
{
	int rc;
	for (Node *node = this; node; node = node->parent) {
		assert(node->refCount);
		if (node == this) {
			rc = --node->refCount;
		} else {
			--node->refCount;
		}
	}
	return rc;
}

/******************************************************/
/* VFS::Mount class */

VFS::Mount::Mount(BlkDevice *dev, const char *fsType)
{
	dev->AddRef();
	this->dev = dev;
	fs = DeviceFS::Create(dev, fsType);
}

VFS::Mount::~Mount()
{
	dev->Release();
}

/******************************************************/
/* VFS::File class */

VFS::File::File()
{
}

VFS::File::~File()
{

}
