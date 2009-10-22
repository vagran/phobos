/*
 * /kernel/sys/queue.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#ifndef QUEUE_H_
#define QUEUE_H_
#include <sys.h>
phbSource("$Id$");


/* Linked lists */

struct _ListEntry;
typedef struct _ListEntry ListEntry;
struct _ListEntry {
	ListEntry *next,*prev;
};

typedef struct {
	ListEntry *first;
} ListHead;

#define LIST_INIT(head) {(head).first = 0;}

#define LIST_INITENTRY(entry, value) { \
	(value)->entry.next = &(value)->entry; \
	(value)->entry.prev = &(value)->entry; \
}

#define LIST_SET(head, entry, value) {(head).first = &(value)->entry;}

#define LIST_ISEMPTY(head) (!(head).first)

#define LIST_ISFIRST(entry, value, head) (&(value)->entry == (head).first)

#define LIST_ISLAST(entry, value, head) ((value)->entry.next == (head).first)

#define LIST_FIRST(type, entry, head) (LIST_DATA(type, entry, (head).first))

#define LIST_LAST(type, entry, head) (LIST_DATA(type, entry, (head).first ? (head).first->prev : 0))

#define LIST_NEXT(type, entry, value, head) (LIST_DATA(type, entry, (value)->entry.next))

#define LIST_PREV(type, entry, value, head) (LIST_DATA(type, entry, (value)->entry.prev))

#define LIST_DATA(type, entry, value) ((value) ? ((type *)(((u8 *)(value)) - OFFSETOF(type, entry))) : 0)

#define LIST_FOREACH(type, entry, var, head) for ( \
	(var) = LIST_DATA(type, entry, (head).first); \
	(var); \
	(var) = (((var)->entry.next == (head).first || !(head).first) ? 0 : LIST_DATA(type, entry, (var)->entry.next)))

#define LIST_ADD(entry, var, head) { \
	if ((head).first) { \
		(var)->entry.next = (head).first; \
		(var)->entry.prev = (head).first->prev; \
		(head).first->prev->next = &(var)->entry; \
		(head).first->prev = &(var)->entry; \
	} else { \
		(var)->entry.next = &(var)->entry; \
		(var)->entry.prev = &(var)->entry; \
		(head).first = &(var)->entry; \
	} \
}

#define LIST_ADDFIRST(entry, var, head) {\
	LIST_ADD(entry, var, head); \
	(head).first = &(var)->entry; \
}

#define LIST_INSERTAFTER(entry, var, after) { \
	(var)->entry.next = (after)->entry.next; \
	(var)->entry.prev = &(after)->entry; \
	(after)->entry.next->prev = &(var)->entry; \
	(after)->entry.next = &(var)->entry; \
}

#define LIST_INSERTBEFORE(entry, var, head, before) { \
	(var)->entry.next = &(before)->entry; \
	(var)->entry.prev = (before)->entry.prev; \
	(before)->entry.prev->next = &(var)->entry; \
	(before)->entry.prev = &(var)->entry; \
	if ((head).first == &(before)->entry) { \
		(head).first = &(var)->entry; \
	} \
}

#define LIST_ADDLAST LIST_ADD

#define LIST_DELETE(entry, var, head) { \
	if ((head).first) { \
		if (&(var)->entry == (head).first) { \
			(head).first = (var)->entry.next; \
		} \
		if (&(var)->entry == (head).first) {\
			(head).first = 0; \
		} else { \
			(var)->entry.next->prev = (var)->entry.prev; \
			(var)->entry.prev->next = (var)->entry.next; \
		} \
	} \
}


/*
 * Binary trees
 * Supported types for key are u8, u16, u32, u64 (see CompilerStub implementation).
 */

template <typename key_t>
class Tree {
public:
	struct _TreeEntry;
	typedef struct _TreeEntry TreeEntry;
	struct _TreeEntry {
		key_t key;
		key_t mask; /* branch left when branching bit is clear */
		TreeEntry *left, *right, *parent;
	};

	typedef struct {
		TreeEntry *rootnode;
	} TreeRoot;

	static TreeEntry *FindNode(TreeRoot &root, key_t key);
	static void AddNode(TreeRoot &root, TreeEntry *node); /* key must be set in 'node' */
	static void DeleteNode(TreeRoot &root, TreeEntry *node);
	static TreeEntry *GetNextNode(TreeRoot &root, TreeEntry *node);
	static int CheckTree(TreeRoot &root);

	static void CompilerStub(); /* do not call it! It is for internal magic. */
};

#define TREE_INIT(root) {(root).rootnode = 0;}

#define TREE_ISEMPTY(root) (!(root).rootnode)

#define TREE_DATA(type, entry, value) ((value) ? ((type *)(((u8 *)(value)) - OFFSETOF(type, entry))) : 0)

#define TREE_ROOT(type, entry, root) TREE_DATA(type, entry, (root).rootnode)

#define TREE_LEFT(type, entry, value) ((value)->entry.left ? TREE_DATA(type, entry, (value)->entry.left) : 0)

#define TREE_RIGHT(type, entry, value) ((value)->entry.right ? TREE_DATA(type, entry, (value)->entry.right) : 0)

#define TREE_PARENT(type, entry, value) ((value)->entry.parent ? TREE_DATA(type, entry, (value)->entry.parent) : 0)

#define TREE_FIND(keyValue, type, entry, var, root) \
	((var) = (type *)Tree<typeof ((var)->entry.key)>::FindNode(root, keyValue), (var) = TREE_DATA(type, entry, var))

#define TREE_ADD(entry, var, root) Tree<typeof ((var)->entry.key)>::AddNode((root), &(var)->entry)

#define TREE_DELETE(entry, var, root) Tree<typeof ((var)->entry.key)>::DeleteNode((root), &(var)->entry)

#define TREE_FOREACH(type, entry, var, root) for ( \
	(var) = TREE_DATA(type, entry, Tree<typeof ((var)->entry.key)>::GetNextNode(root, 0)); \
	(var); \
	(var) = TREE_DATA(type, entry, Tree<typeof ((var)->entry.key)>::GetNextNode(root, &(var)->entry)))


#endif /* QUEUE_H_ */
