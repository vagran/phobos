/*
 * /kernel/sys/queue.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
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

#define LIST_ADD_LAST	LIST_ADD

#define LIST_ADD_FIRST(entry, var, head) {\
	LIST_ADD(entry, var, head); \
	(head).first = &(var)->entry; \
}

#define LIST_INSERT_AFTER(entry, var, after) { \
	(var)->entry.next = (after)->entry.next; \
	(var)->entry.prev = &(after)->entry; \
	(after)->entry.next->prev = &(var)->entry; \
	(after)->entry.next = &(var)->entry; \
}

#define LIST_INSERT_BEFORE(entry, var, head, before) { \
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
	if (!LIST_ISEMPTY(head)) { \
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
					if (LIST_ISFIRST(entry, var1, head)) {\
						LIST_ADD_FIRST(entry, var2, head); \
						break; \
					} \
					var1 = LIST_PREV(type, entry, var1); \
					if (!(condition)) { \
						LIST_INSERT_AFTER(entry, var2, var1); \
						break; \
					} \
				} \
				var1 = __Xls_var; \
			} else { \
				var1 = var2; \
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
	static TreeEntry *AddNode(TreeRoot &root, TreeEntry *node) __noinline; /* key must be set in 'node' */
	static void DeleteNode(TreeRoot &root, TreeEntry *node) __noinline;
	static TreeEntry *GetNextNode(TreeRoot &root, TreeEntry *node) __noinline;
	static int CheckTree(TreeRoot &root) __noinline;
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

#define TREE_ADD(entry, var, root, keyValue) ({ \
	(var)->entry.key = keyValue; \
	Tree<typeof ((var)->entry.key)>::AddNode((root), &(var)->entry); \
})

#define TREE_REPLACE(entry, var, root, newvar) { \
	(newvar)->entry.key = (var)->entry.key; \
	(newvar)->entry.mask = (var)->entry.mask; \
	if ((var)->entry.parent) { \
		if ((var)->entry.parent->right == &(var)->entry) { \
			(var)->entry.parent->right = &(newvar)->entry; \
		} else { \
			assert((var)->entry.parent->left == &(var)->entry); \
			(var)->entry.parent->left = &(newvar)->entry; \
		} \
	} \
	(newvar)->entry.right = (var)->entry.right; \
	if ((var)->entry.right) { \
		assert((var)->entry.right->parent == &(var)->entry); \
		(var)->entry.right->parent = &(newvar)->entry; \
	} \
	(newvar)->entry.left = (var)->entry.left; \
	if ((var)->entry.left) { \
		assert((var)->entry.left->parent == &(var)->entry); \
		(var)->entry.left->parent = &(newvar)->entry; \
	} \
}

#define TREE_DELETE(entry, var, root) Tree<typeof ((var)->entry.key)>::DeleteNode((root), &(var)->entry)

#define TREE_FIRST(type, entry, root) TREE_DATA(type, entry, \
	Tree<typeof (((type *)1)->entry.key)>::GetNextNode(root, 0))

#define TREE_NEXT(type, entry, var, root) TREE_DATA(type, entry, \
	Tree<typeof ((var)->entry.key)>::GetNextNode(root, &(var)->entry))

#define TREE_FOREACH(type, entry, var, root) for ( \
	(var) = TREE_FIRST(type, entry, root); \
	(var); \
	(var) = TREE_NEXT(type, entry, var, root))

/*
 * String trees
 *
 */

template <typename char_t = char>
class StringTree {
public:
	typedef u32 hash_t;

	struct _TreeEntry;
	typedef struct _TreeEntry TreeEntry;
	struct _TreeEntry {
		char_t *key;
		int keyLen; /* number of characters in the key */
		Tree<hash_t>::TreeEntry tree;/* string hash in key */
		TreeEntry *prev, *next;
	};

	typedef struct {
		Tree<hash_t>::TreeRoot rootnode;
	} TreeRoot;

	static inline int KeyCompare(const char_t *key1, int len1,
		const char_t *key2, int len2) {
		if (len1 != len2) {
			return 1;
		}
		while (len1) {
			if (*key1 != *key2) {
				return 1;
			}
			key1++;
			key2++;
			len1--;
		}
		return 0;
	}

	static TreeEntry *FindNode(TreeRoot &root, char_t *key,
		int keyLen = -1) __noinline;
	static void AddNode(TreeRoot &root, TreeEntry *node, char_t *key,
		int keyLen = -1) __noinline;
	static void DeleteNode(TreeRoot &root, TreeEntry *node) __noinline;
	static TreeEntry *GetNextNode(TreeRoot &root, TreeEntry *node) __noinline;
};

#define STREE_INIT(root)	TREE_INIT(root.rootnode)

#define STREE_ISEMPTY(root)	(!(root).rootnode)

#define STREE_DATA(type, entry, value) ((value) ? ((type *)(((u8 *)(value)) - OFFSETOF(type, entry))) : 0)

#define STREE_KEY(entry, var)	((var)->entry.key)

#define STREE_KEYLEN(entry, var)	((var)->entry.keyLen)

#define STREE_ROOT(type, entry, root) STREE_DATA(type, entry, (root).rootnode)

#define STREE_FIND(type, entry, root, keyValue, ...) \
	({ \
		void *__Xp = StringTree<typeof (*((type *)1)->entry.key)>::FindNode(root, keyValue, ## __VA_ARGS__); \
		STREE_DATA(type, entry, __Xp); \
	})

#define STREE_ADD(entry, var, root, keyValue, ...) ({ \
	StringTree<typeof (*(var)->entry.key)>::AddNode((root), &(var)->entry, keyValue, ## __VA_ARGS__); \
})

#define STREE_DELETE(entry, var, root) StringTree<typeof (*(var)->entry.key)>::DeleteNode((root), &(var)->entry)

#define STREE_FIRST(type, entry, root) STREE_DATA(type, entry, \
	StringTree<typeof (*((type *)1)->entry.key)>::GetNextNode(root, 0))

#define STREE_NEXT(type, entry, var, root) STREE_DATA(type, entry, \
	StringTree<typeof (*(var)->entry.key)>::GetNextNode(root, &(var)->entry))

#define STREE_FOREACH(type, entry, var, root) for ( \
	(var) = STREE_FIRST(type, entry, root); \
	(var); \
	(var) = STREE_NEXT(type, entry, var, root))

#endif /* QUEUE_H_ */
