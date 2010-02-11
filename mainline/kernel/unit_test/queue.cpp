/*
 * /kernel/unit_test/queue.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
#include "CTest.h"

class QueueTest : public CTest {
public:
	QueueTest(const char *name, const char *desc);
	virtual int Run();
};

QueueTest::QueueTest(const char *name, const char *desc) : CTest(name, desc)
{
}

int
QueueTest::Run()
{
	ListHead head;
	int pat1[] = {0, 1, 2, 3, 4, 5, 6, 7};
	int pat2[] = {0, 1, 2, 3, 8, 4, 5, 6, 7};
	int pat3[] = {0, 1, 2, 9, 3, 8, 4, 5, 6, 7};
	int pat4[] = {0, 1, 2, 9, 8, 4, 5, 6, 7};
	int pat5[] = {4, 6, 2, 1, 5, 0, 7, 9, 3, 8};
	int k;

	typedef struct {
		ListEntry list;
		int idx;
	} Item;

	Item *p, *p1 = 0;

	LIST_INIT(head);

	for (int i = 0; i < 8; i++) {
		p = UTALLOC(Item, 1);
		LIST_ADD(list, p, head);
		p->idx = i;
	}

	k = 0;
	ut_printf("Simple add...");
	LIST_FOREACH(Item, list, p, head) {
		if (p->idx != pat1[k]) {
			ut_printf("%d. %d != %d\n", k, p->idx, pat1[k]);
			return -1;
		}
		if (p->idx == 3) {
			p1 = p;
		}
		k++;
	}
	if (k != sizeof(pat1) / sizeof(pat1[0])) {
		ut_printf("Incorrect list length, expected %d, actual %d\n",
			sizeof(pat1) / sizeof(pat1[0]), k);
	}
	ut_printf("ok\n");

	ut_printf("Insert after...");
	p = UTALLOC(Item, 1);
	p->idx = 8;
	LIST_INSERTAFTER(list, p, p1);
	k = 0;
	LIST_FOREACH(Item, list, p, head) {
		if (p->idx != pat2[k]) {
			ut_printf("%d. %d != %d\n", k, p->idx, pat2[k]);
			return -1;
		}
		k++;
	}
	if (k != sizeof(pat2) / sizeof(pat2[0])) {
		ut_printf("Incorrect list length, expected %d, actual %d\n",
			sizeof(pat2) / sizeof(pat2[0]), k);
	}
	ut_printf("ok\n");

	ut_printf("Insert before...");
	p = UTALLOC(Item, 1);
	p->idx = 9;
	LIST_INSERTBEFORE(list, p, head, p1);
	k = 0;
	LIST_FOREACH(Item, list, p, head) {
		if (p->idx != pat3[k]) {
			ut_printf("%d. %d != %d\n", k, p->idx, pat3[k]);
			return -1;
		}
		k++;
	}
	if (k != sizeof(pat3) / sizeof(pat3[0])) {
		ut_printf("Incorrect list length, expected %d, actual %d\n",
			sizeof(pat3) / sizeof(pat3[0]), k);
	}
	ut_printf("ok\n");

	ut_printf("Delete...");
	LIST_DELETE(list, p1, head);
	k = 0;
	LIST_FOREACH(Item, list, p, head) {
		if (p->idx != pat4[k]) {
			ut_printf("%d. %d != %d\n", k, p->idx, pat4[k]);
			return -1;
		}
		k++;
	}

	if (k != sizeof(pat4) / sizeof(pat4[0])) {
		ut_printf("Incorrect list length, expected %d, actual %d\n",
			sizeof(pat4) / sizeof(pat4[0]), k);
	}
	ut_printf("ok\n");

	ut_printf("Reverse iteration...");
	k = sizeof(pat4) / sizeof(pat4[0]) - 1;
	LIST_FOREACH_REVERSE(Item, list, p) {
		if (k < 0) {
			ut_printf("iteration underflow\n");
			return -1;
		}
		if (p->idx != pat4[k]) {
			ut_printf("%d. %d != %d\n", k, p->idx, pat4[k]);
			return -1;
		}
		k--;
	}
	if (k != -1) {
		ut_printf("Incorrect list length, expected %d, actual %d\n",
				sizeof(pat4) / sizeof(pat4[0]), sizeof(pat4) / sizeof(pat4[0]) - k);
	}
	ut_printf("ok\n");

	ut_printf("Sorting...");
	LIST_INIT(head);
	for (int i = 0; i < 10; i++) {
		p = UTALLOC(Item, 1);
		LIST_ADD(list, p, head);
		p->idx = pat5[i];
	}
	LIST_SORT(Item, list, p1, p2, head, p2->idx < p1->idx);

	k = 0;
	LIST_FOREACH(Item, list, p, head) {
		if (p->idx != k) {
			ut_printf("%d. %d != %d\n", k, p->idx, k);
			return -1;
		}
		k++;
	}
	ut_printf("ok\n");
	return 0;
}

MAKETEST(QueueTest, "Linked lists", "Linked lists verification test");

/********************************************************************************/

class TreeTest : public CTest {
public:
	TreeTest(const char *name, const char *desc);
	virtual int Run();
};

TreeTest::TreeTest(const char *name, const char *desc) : CTest(name, desc)
{
}

int
TreeTest::Run()
{
	static const int numItems = 32 * 1024;
	static int buf[numItems];
	typedef struct
	{
		Tree<u32>::TreeEntry tree;
		int data;
	} Item;
	static Item items[numItems];
	Item *p;
	Tree<u32>::TreeRoot root;

	TREE_INIT(root);

	ut_memset(buf, 0, sizeof(buf));
	int numLeft = numItems;
	while (numLeft) {
		int idx = (int)((u64)ut_rand() * (numItems - 1) / UT_RAND_MAX);
		int idx1 = idx;
		while (1) {
			if (!buf[idx]) {
				buf[idx] = 1;
				break;
			}
			idx++;
			if (idx >= numItems) idx = 0;
			if (idx == idx1) {
				ut_printf("Can't find free item, %d should be left\n",
				    numLeft);
				return -1;
			}
		}
		items[idx].data = idx;
		TREE_ADD(tree, &items[idx], root, idx);
		numLeft--;
	}

	int check1 = Tree<u32>::CheckTree(root);
	if (check1) ut_printf("Initial tree check failed\n");

	ut_printf("Verifying tree searching...\n");
	int numFound = 0, numValid = 0;
	for (int i = 0; i < numItems; i++) {
		p = TREE_FIND(i, Item, tree, root);
		if (!p) {
			ut_printf("Item not found, index=%d\n", i);
			continue;
		}
		numFound++;
		if (p->data != i) {
			ut_printf("Invalid item found, data=%d, should be %d\n",
			    p->data, i);
			continue;
		}
		numValid++;
	}
	ut_printf("%d found, %d valid, %d total\n", numFound, numValid, numItems);

	ut_printf("Verifying tree walking...\n");
	ut_memset(buf, 0, sizeof(buf));
	int numDup = 0;
	TREE_FOREACH(Item, tree, p, root) {
		if (buf[p->data]) {
			ut_printf("Duplicated enumeration, index=%d\n", p->data);
			numDup++;
		}
		buf[p->data]++;
	}
	int numNonwalked = 0;
	for (int i = 0; i < numItems; i++) {
		if (!buf[i]) {
			ut_printf("Non-enumerated item, index=%d\n", i);
			numNonwalked++;
		}
	}
	ut_printf(
	    "%d items enumerated more than once, %d items not enumerated\n",
	    numDup, numNonwalked);

	ut_printf("Verifying deleting node...\n");
	ut_memset(buf, 0, sizeof(buf));
	for (int i = 0; i < 256; i++) {
		int iDel = (int)((u64)ut_rand() * numItems / UT_RAND_MAX);
		while (buf[iDel]) {
			iDel++;
			if (iDel >= numItems) iDel = 0;
		}
		buf[iDel] = 1;
		TREE_DELETE(tree, &items[iDel], root);
	}
	/* searching should fail */
	int numDelFound = 0;
	for (int i = 0; i < numItems; i++) {
		if (buf[i]) {
			p = TREE_FIND(i, Item, tree, root);
			if (p) {
				ut_printf("Deleted node found, index=%d\n", p->data);
				numDelFound++;
			}
		}
	}
	ut_printf("%d deleted nodes found\n", numDelFound);
	int numDelDup = 0, numDelEnumed = 0;
	TREE_FOREACH(Item, tree, p, root) {
		if (buf[p->data] & 2) {
			ut_printf("Duplicated enumeration, index=%d\n", p->data);
			numDelDup++;
		}
		buf[p->data]|=2;
		if (buf[p->data] & 1) {
			ut_printf("Deleted node enumerated, index=%d\n", p->data);
			numDelEnumed++;
		}
	}
	ut_printf(
	    "%d deleted nodes found, %d deleted nodes enumerated, %d duplicated enumerations\n",
	    numDelFound, numDelEnumed, numDelDup);

	int numDelNonwalked = 0, numDelNotFound = 0;
	for (int i = 0; i < numItems; i++) {
		if (!(buf[i] & 1)) {
			if (!(buf[i] & 2)) {
				ut_printf("Non-enumerated not deleted item, index=%d\n", i);
				numDelNonwalked++;
			}
			p = TREE_FIND(i, Item, tree, root);
			if (!p) {
				ut_printf("Not found not deleted item, index=%d\n", i);
				numDelNotFound++;
			}
		}
	}
	ut_printf(
	    "%d not deleted items not enumerated, %d not deleted items not found\n",
	    numDelNonwalked, numDelNotFound);

	int check2 = Tree<u32>::CheckTree(root);
	if (check2) {
		ut_printf("Final tree check failed\n");
	}

	return (!numDelNotFound && !numDelNonwalked && !numDelEnumed && !numDelDup
	    && !check1 && !check2 && !numDup && !numDelFound && numFound
	    == numItems && numValid == numItems && !numNonwalked) ? 0 : -1;
	return 0;
}

MAKETEST(TreeTest, "Binary trees", "Binary trees verification test");
