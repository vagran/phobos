/*
 * /kernel/lib/Tree.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

template <typename key_t>
void
Tree<key_t>::AddNode(TreeRoot &root, TreeEntry *node)
{
	if (!root.rootnode) {
		node->left = 0;
		node->right = 0;
		node->parent = 0;
		node->mask = 1;
		root.rootnode = node;
		return;
	}
	TreeEntry *p = root.rootnode;
	while (1) {
		if (p->key == node->key) {
			return;
		}
		if (node->key & p->mask) {
			if (!p->right) {
				p->right = node;
				break;
			}
			p = p->right;
		} else {
			if (!p->left) {
				p->left = node;
				break;
			}
			p = p->left;
		}
	}
	node->left = 0;
	node->right = 0;
	node->parent = p;
	node->mask = p->mask << 1;
}

template <typename key_t>
void
Tree<key_t>::DeleteNode(TreeRoot &root, TreeEntry *node)
{
	/* find node to swap with */
	TreeEntry *p;
	if (node->left) {
		p = node->left;
	} else if (node->right) {
		p = node->right;
	} else {
		p = 0;
	}

	if (p) {
		while (p->left || p->right) {
			if (p->left) {
				p = p->left;
			} else {
				p = p->right;
			}
		}
		p->mask = node->mask;
		if (p->parent->left == p) {
			p->parent->left = 0;
		} else {
			p->parent->right = 0;
		}
		p->left = node->left;
		if (node->left) {
			node->left->parent = p;
		}
		p->right = node->right;
		if (node->right) {
			node->right->parent = p;
		}
		p->parent = node->parent;
	}

	if (node->parent) {
			if (node->parent->left == node) {
				node->parent->left = p;
			} else {
				node->parent->right = p;
			}
	} else {
		root.rootnode = p;
	}
}

template <typename key_t>
typename Tree<key_t>::TreeEntry *
Tree<key_t>::FindNode(TreeRoot &root, key_t key)
{
	TreeEntry *p = root.rootnode;
	while (p && p->key != key) {
		if (key & p->mask) {
			p = p->right;
		} else {
			p = p->left;
		}
	}
	return p;
}

template <typename key_t>
typename Tree<key_t>::TreeEntry *
Tree<key_t>::GetNextNode(TreeRoot &root, TreeEntry *node)
{
	if (!root.rootnode) {
		return 0;
	}
	if (!node) {
		return root.rootnode;
	}
	if (node->left) {
		return node->left;
	}
	if (node->right) {
		return node->right;
	}

	while (1) {
		if (!node->parent) {
			return 0;
		}
		if (node->parent->right == node) {
			node = node->parent;
			continue;
		}
		node = node->parent;
		if (node->right) {
			return node->right;
		}
	}
}

template <typename key_t>
int
Tree<key_t>::CheckTree(TreeRoot &root)
{
	TreeEntry *p = 0;
	while ((p = GetNextNode(root, p))) {
		if (!p->parent) {
			continue;
		}
		if (p->key & p->parent->mask) {
			if (p->parent->right != p) {
				return -1;
			}
		} else {
			if (p->parent->left != p) {
				return -1;
			}
		}
		if (p->left) {
			if ((p->key & (p->mask - 1)) != (p->left->key & (p->mask - 1))) {
				return -1;
			}
		}
		if (p->right) {
			if ((p->key & (p->mask - 1)) != (p->right->key & (p->mask - 1))) {
				return -1;
			}
		}
	}
	return 0;
}

/* Force compiler to compile all required versions of Tree class methods */

void
TreeCompilerStub()
{
	/* all supported types are here */
	Tree<u8>::CompilerStub();
	Tree<u16>::CompilerStub();
	Tree<u32>::CompilerStub();
	Tree<u64>::CompilerStub();
}

template <typename key_t>
__volatile void
Tree<key_t>::CompilerStub()
{
	TreeRoot root;
	TREE_INIT(root);
	TreeEntry node;
	memset(&node, 0, sizeof(node));
	key_t key = 0;

	Tree<key_t>::AddNode(root, &node);
	Tree<key_t>::DeleteNode(root, &node);
	Tree<key_t>::FindNode(root, key);
	Tree<key_t>::GetNextNode(root, &node);
	Tree<key_t>::CheckTree(root);
}
