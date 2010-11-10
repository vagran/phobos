/*
 * /phobos/kernel/vfs/vfs.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

VFS *vfs;

VFS::VFS()
{
	root = AllocateNode(0, Node::T_DIRECTORY, ".");
	ensure(root);
	root->Release(); /* release initial reference */
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
	node->AddRef();
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
	treeLock.Lock();
	if (parent->numChilds < 32) {
		LIST_FOREACH(Node, list, node, parent->childs) {
			if (node->hash != hash) {
				continue;
			}
			if (nameLen == -1) {
				if (!strncmp(name, node->name, nameLen) && !node->name[nameLen]) {
					node->AddRef();
					treeLock.Unlock();
					return node;
				}
			} else {
				if (!strcmp(name, node->name)) {
					node->AddRef();
					treeLock.Unlock();
					return node;
				}
			}
		}
		treeLock.Unlock();
		return 0;
	}
	node = TREE_FIND(hash, Node, hashTreeEntry, parent->hashTree);
	if (!node) {
		treeLock.Unlock();
		return 0;
	}
	if (nameLen == -1) {
		if (!strncmp(name, node->name, nameLen) && !node->name[nameLen]) {
			node->AddRef();
			treeLock.Unlock();
			return node;
		}
	} else {
		if (!strcmp(name, node->name)) {
			node->AddRef();
			treeLock.Unlock();
			return node;
		}
	}
	Node *hashHead = node;
	LIST_FOREACH(Node, hashListEntry, node, hashHead->hashList) {
		if (nameLen == -1) {
			if (!strncmp(name, node->name, nameLen) && !node->name[nameLen]) {
				node->AddRef();
				treeLock.Unlock();
				return node;
			}
		} else {
			if (!strcmp(name, node->name)) {
				node->AddRef();
				treeLock.Unlock();
				return node;
			}
		}
	}
	treeLock.Unlock();
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
	Node *lastNode = root;
	lastNode->AddRef();
	while (next && *next && lastNode) {
		for (cp = next; *cp && *cp == '/'; cp++);
		if (!*cp) {
			break;
		}
		for (next = cp; *next && *next != '/'; next++);
		Node *node = GetSubNode(lastNode, cp, next - cp);
		lastNode->Release();
		lastNode = node;
	}
	return lastNode;
}

VFS::Mount *
VFS::CreateMount(BlkDevice *dev, int flags, const char *fsType)
{
	Mount *p = NEW(Mount, dev, flags, fsType);
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
VFS::MountDevice(BlkDevice *dev, const char *mountPoint, int flags, const char *type)
{
	assert(dev->GetType() == Device::T_BLOCK);
	Mount *m = CreateMount(dev, flags, type);
	if (!m) {
		return -1;
	}
	Node *node = LookupNode(mountPoint);
	if (!node) {
		m->Release();
		klog(KLOG_WARNING, "Mount point not found: '%s'", mountPoint);
		return 0;
	}
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
	if (node == root) {
		root = mNode;
	}
	mNode->Release(); /* release initial reference */
	return 0;
}

VFS::File *
VFS::CreateFile(const char *path, int flags, Node::Type type)
{
	File *file;

	Node *node = LookupNode(path);
	if (node) {
		if (node->type != type) {
			ERROR(E_EXISTS, "The file of another type already exists "
				"(requested type = %d, existing type = %d)", type, node->type);
			node->Release();
			return 0;
		}
	} else {
		if (flags & CFF_EXISTING) {
			ERROR(E_NOTFOUND, "Specified file does not exist ('%s')", path);
			return 0;
		}
		/* Create new node */
		int len;
		for (len = strlen(path); len && path[len - 1] == '/'; len--);
		int nameEnd = len;
		for (; len && path[len - 1] != '/'; len--);
		int nameStart = len;
		for (; len && path[len - 1] == '/'; len--);
		if (nameStart == nameEnd) {
			ERROR(E_INVAL, "Empty file name in path '%s'", path);
			return 0;
		}
		/* Get parent node */
		Node *parentNode;
		if (len) {
			KString parentPath;
			memcpy(parentPath.LockBuffer(len), path, len);
			parentPath.ReleaseBuffer(len);
			parentNode = LookupNode(parentPath.GetBuffer());
			if (!parentNode) {
				ERROR(E_NOTFOUND, "Parent directory not found (%s)",
					parentPath.GetBuffer());
				return 0;
			}
			if (parentNode->type != Node::T_DIRECTORY) {
				ERROR(E_INVAL, "Parent node is not a directory ('%s' type is %d)",
					parentPath.GetBuffer(), node->type);
				parentNode->Release();
				return 0;
			}
		} else {
			parentNode = root;
			parentNode->AddRef();
		}
		/* Create subnode */
		node = AllocateNode(parentNode, type, &path[nameStart], nameEnd - nameStart);
		parentNode->Release();
		if (!node) {
			ERROR(E_FAULT, "Cannot allocate new node");
			return 0;
		}
	}
	/* XXX should implement file cache management */
	switch (node->type) {
	case Node::T_REGULAR:
		file = NEW(File, node);
		break;
	case Node::T_DIRECTORY:
		file = NEW(Directory, node);
		break;
	default:
		return 0;
	}
	node->Release();
	return file;
}

MM::VMObject *
VFS::MapFile(File *file)
{
	/* XXX should be able to map devices too */
	if (file->GetType() != Node::T_REGULAR) {
		return 0;
	}
	/* XXX should implement file cache management */
	vsize_t size = roundup2(file->GetSize(), PAGE_SIZE);
	MM::VMObject *obj = NEW(MM::VMObject, size, MM::VMObject::F_FILE);
	if (!obj) {
		return 0;
	}
	if (obj->CreatePager(file)) {
		obj->Release();
		return 0;
	}
	return obj;
}

MM::VMObject *
VFS::MapFile(const char *path)
{
	File *file = CreateFile(path);
	if (!file) {
		return 0;
	}
	MM::VMObject *obj = MapFile(file);
	/* release our reference to the file */
	file->Release();
	return obj;
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
	int rc = 0;
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

u32
VFS::Node::GetSize()
{
	if (!mount) {
		return 0;
	}
	DeviceFS *fs = mount->GetFS();
	return fs->GetNodeSize(fsNode);
}

u32
VFS::Node::Read(off_t offset, void *buf, u32 len)
{
	if (!mount) {
		return 0;
	}
	DeviceFS *fs = mount->GetFS();
	return fs->ReadNode(fsNode, offset, len, buf);
}

/******************************************************/
/* VFS::Mount class */

VFS::Mount::Mount(BlkDevice *dev, int flags, const char *fsType)
{
	dev->AddRef();
	this->dev = dev;
	fs = DeviceFS::Create(dev, flags, fsType);
}

VFS::Mount::~Mount()
{
	dev->Release();
}

/******************************************************/
/* VFS::File class */

VFS::File::File(Node *node)
{
	node->AddRef();
	this->node = node;
}

VFS::File::~File()
{
	node->Release();
}

u32
VFS::File::Read(off_t offset, void *buf, u32 len)
{
	return node->Read(offset, buf, len);
}

u32
VFS::File::GetSize()
{
	return node->GetSize();
}

/******************************************************/
/* VFS::Directory class */

VFS::Directory::Directory(Node *node) : File(node)
{

}

VFS::Directory::~Directory()
{
}

u32
VFS::Directory::Read(u64 offset, u32 len, void *buf)
{
	return 0;
}
