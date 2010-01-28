/*
 * /kernel/sys/queue.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
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

#define LIST_LAST(type, entry, head) (LIST_ISEMPTY(head) ? 0 : LIST_DATA(type, entry, (head).first->prev))

#define LIST_NEXT(type, entry, value) (LIST_DATA(type, entry, (value)->entry.next))

#define LIST_PREV(type, entry, value) (LIST_DATA(type, entry, (value)->entry.prev))

#define LIST_DATA(type, entry, value) ((value) ? ((type *)(((u8 *)(value)) - OFFSETOF(type, entry))) : 0)

#define LIST_FOREACH(type, entry, var, head) for ( \
	(var) = LIST_FIRST(type, entry, head); \
	(var); \
	(var) = LIST_ISLAST(entry, var, head) ? 0 : LIST_NEXT(type, entry, var))

#define LIST_FOREACH_REVERSE(type, entry, var) for ( \
	(var) = LIST_LAST(type, entry, head); \
	(var); \
	(var) = LIST_ISFIRST(entry, var, head) ? 0 : LIST_PREV(type, entry, var))

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

#define LIST_ADDLAST	LIST_ADD

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

#define LIST_ROTATE(entry, var, head) {(head).first = &(var)->entry;}

/* condition should use var1 and var2, TRUE if var2 should precede var1 */
#define LIST_SORT(type, entry, var1, var2, head, condition) { \
	type *var1 = LIST_FIRST(type, entry, head); \
	type *var2; \
	while (1) { \
		if (LIST_ISLAST(entry, var1, head)) { \
			break; \
		} \
		var2 = LIST_NEXT(type, entry, var1); \
		if (condition) {\
			type *__Xls_var = var1; \
			LIST_DELETE(entry, var2, head); \
			while (1) { \
				if (LIST_ISFIRST(entry, __Xls_var, head)) {\
					LIST_ADDFIRST(entry, var2, head); \
					break; \
				} \
				__Xls_var = LIST_PREV(type, entry, __Xls_var); \
				if (!(condition)) { \
					LIST_INSERTAFTER(entry, var2, __Xls_var); \
					break; \
				} \
			} \
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

	static TreeEntry *FindNode(TreeRoot &root, key_t key) __noinline;
	static void AddNode(TreeRoot &root, TreeEntry *node) __noinline; /* key must be set in 'node' */
	static void DeleteNode(TreeRoot &root, TreeEntry *node) __noinline;
	static TreeEntry *GetNextNode(TreeRoot &root, TreeEntry *node) __noinline;
	static int CheckTree(TreeRoot &root) __noinline;

	static __volatile void CompilerStub(); /* do not call it! It is for internal magic. */
};

#define TREE_INIT(root) {(root).rootnode = 0;}

#define TREE_ISEMPTY(root) (!(root).rootnode)

#define TREE_DATA(type, entry, value) ((value) ? ((type *)(((u8 *)(value)) - OFFSETOF(type, entry))) : 0)

#define TREE_KEY(entry, var)	((var)->entry.key)

#define TREE_ROOT(type, entry, root) TREE_DATA(type, entry, (root).rootnode)

#define TREE_LEFT(type, entry, value) TREE_DATA(type, entry, (value)->entry.left)

#define TREE_RIGHT(type, entry, value) TREE_DATA(type, entry, (value)->entry.right)

#define TREE_PARENT(type, entry, value) TREE_DATA(type, entry, (value)->entry.parent)

#define TREE_FIND(keyValue, type, entry, root) \
	({ \
		void *__Xp = Tree<typeof ((root).rootnode->key)>::FindNode(root, keyValue); \
		TREE_DATA(type, entry, __Xp); \
	})

#define TREE_ADD(entry, var, root, keyValue) { \
	(var)->entry.key = keyValue; \
	Tree<typeof ((var)->entry.key)>::AddNode((root), &(var)->entry); \
}

#define TREE_DELETE(entry, var, root) Tree<typeof ((var)->entry.key)>::DeleteNode((root), &(var)->entry)

#define TREE_FOREACH(type, entry, var, root) for ( \
	(var) = TREE_DATA(type, entry, Tree<typeof ((var)->entry.key)>::GetNextNode(root, 0)); \
	(var); \
	(var) = TREE_DATA(type, entry, Tree<typeof ((var)->entry.key)>::GetNextNode(root, &(var)->entry)))


#endif /* QUEUE_H_ */
