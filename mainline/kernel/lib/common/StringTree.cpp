/*
 * /phobos/kernel/lib/common/StringTree.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

template <typename char_t>
void
StringTree<char_t>::AddNode(TreeRoot &root, TreeEntry *node, char_t *key,
		int keyLen)
{
	if (keyLen == -1) {
		for (keyLen = 0; key[keyLen]; keyLen++);
	}

	Tree<hash_t>::TreeEntry *newNode = TREE_ADD(tree, node, root.rootnode,
		gethash((u8 *)key, sizeof(char_t) * keyLen));
	if (newNode == &node->tree) {
		node->prev = node;
		node->next = node;
	} else {
		/* hash collision */
		TreeEntry *prevNode = TREE_DATA(TreeEntry, tree, newNode);
		node->prev = prevNode;
		node->next = prevNode->next;
		prevNode->next->prev = node;
		prevNode->next = node;
		TREE_KEY(tree, node) = 0;
	}
	node->key = key;
	node->keyLen = keyLen;
}

template <typename char_t>
void
StringTree<char_t>::DeleteNode(TreeRoot &root, TreeEntry *node)
{
	if (node->next != node) {
		/* in a collided node */
		if (TREE_KEY(tree, node)) {
			TREE_REPLACE(tree, node, root.root, node->next);
		}
		node->next->prev = node->prev;
		node->prev->next = node->next;
	} else {
		TREE_DELETE(tree, node, root.rootnode);
	}
}

template <typename char_t>
typename StringTree<char_t>::TreeEntry *
StringTree<char_t>::GetNextNode(TreeRoot &root, TreeEntry *node)
{
	if (node) {
		if (node->next && !TREE_KEY(tree, node->next)) {
			return node->next;
		}
	} else {
		return TREE_FIRST(TreeEntry, tree, root.rootnode);
	}
	return TREE_NEXT(TreeEntry, tree, node->next ? node->next : node, root.rootnode);
}

template <typename char_t>
typename StringTree<char_t>::TreeEntry *
StringTree<char_t>::FindNode(TreeRoot &root, char_t *key, int keyLen)
{
	if (keyLen == -1) {
		for (keyLen = 0; key[keyLen]; keyLen++);
	}
	hash_t hash = gethash((u8 *)key, sizeof(char_t) * keyLen);
	TreeEntry *node = TREE_FIND(hash, TreeEntry, tree, root.rootnode);
	if (!node) {
		return 0;
	}
	if (node->next == node) {
		/* not collided node */
		assert(!KeyCompare(node->key, node->keyLen, key, keyLen));
		return node;
	}
	/* collided node */
	TreeEntry *next = node;
	do {
		if (!KeyCompare(next->key, next->keyLen, key, keyLen)) {
			return next;
		}
		next = next->next;
	} while (next != node);
	return 0;
}

/* Force compiler to compile all required versions of StringSTree class methods */

__volatile void
StringTreeCompilerStub()
{
	/* all supported types are here */
	StringTree<char>::CompilerStub();
	StringTree<u8>::CompilerStub();
	StringTree<u16>::CompilerStub();
	StringTree<u32>::CompilerStub();
	StringTree<u64>::CompilerStub();
}

template <typename char_t>
__volatile void
StringTree<char_t>::CompilerStub()
{
	TreeRoot root;
	STREE_INIT(root);
	TreeEntry node;
	memset(&node, 0, sizeof(node));

	StringTree<char_t>::AddNode(root, &node, 0);
	StringTree<char_t>::DeleteNode(root, &node);
	StringTree<char_t>::FindNode(root, 0);
	StringTree<char_t>::GetNextNode(root, 0);
}
