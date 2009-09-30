/*
 * /kernel/lib/Tree32.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

void
Tree32::AddNode(TreeHead &head, TreeEntry *node)
{
	if (!head) {
		node->left = 0;
		node->right = 0;
		node->parent = 0;
		node->mask = 1;
		head = node;
		return;
	}
	TreeEntry *p = head;
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

void
Tree32::DeleteNode(TreeHead &head, TreeEntry *node)
{
	//find node to swap with
	TreeEntry *p;
	if (node->left) p=node->left;
	else if (node->right) p=node->right;
	else p=0;

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
		head = p;
	}
}

TreeEntry *
Tree32::FindNode(TreeHead head, u32 key)
{
	TreeEntry *p = head;
	while (p && p->key != key) {
		if (key & p->mask) {
			p = p->right;
		} else {
			p = p->left;
		}
	}
	return p;
}

TreeEntry *
Tree32::GetNextNode(TreeHead head, TreeEntry *node)
{
	if (!head) {
		return 0;
	}
	if (!node) {
		return head;
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
	return 0;//make compiler happy
}

int
Tree32::CheckTree(TreeHead head)
{
	TreeEntry *p = 0;
	while ((p = GetNextNode(head, p))) {
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
