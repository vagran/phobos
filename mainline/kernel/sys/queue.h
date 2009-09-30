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

struct _ListEntry;
typedef struct _ListEntry ListEntry;
struct _ListEntry {
	ListEntry *next,*prev;
};

typedef ListEntry *ListHead;

#define XLIST_INIT(head) {(head)=0;}

#define XLIST_INITENTRY(entry,value) {value->entry.next=&value->entry;value->entry.prev=&value->entry;}

#define XLIST_SET(head,entry,value) {(head)=&value->entry;}

#define XLIST_ISEMPTY(head) (!(head))

#define XLIST_ISFIRST(entry,value,head) (&(value)->entry==(head))

#define XLIST_ISLAST(entry,value,head) ((value)->entry.next==(head))

#define XLIST_FIRST(type,entry,head) (XLIST_DATA(type,entry,head))

#define XLIST_LAST(type,entry,head) (XLIST_DATA(type,entry,(head)->prev))

#define XLIST_NEXT(type,entry,value,head) (XLIST_DATA(type,entry,(value)->entry.next))

#define XLIST_PREV(type,entry,value,head) (XLIST_DATA(type,entry,(value)->entry.prev))

#define XLIST_DATA(type,entry,value) ((value) ? ((type *)(((uint8_t *)(value))-OFFSETOF(type,entry))) : 0)

#define XLIST_FOREACH(type,entry,var,head) for (var=(head ? XLIST_DATA(type,entry,head) : 0); \
	var; var=(((var)->entry.next==(head) || !(head)) ? 0 : XLIST_DATA(type,entry,(var)->entry.next)))

#define XLIST_FREE(type,entry,var,head) { if (head) { \
	(var)=XLIST_DATA(type,entry,head); \
	do { \
		xListEntry *_xLF_next=(var)->entry.next;

#define XLIST_FREE_END(type,entry,var,head) \
		(var)=XLIST_DATA(type,entry,_xLF_next); \
	} while (&(var)->entry!=(head) && (head)); \
} \
}

#define XLIST_ADD(entry,var,head) { \
	if (head) { \
		(var)->entry.next=(head); \
		(var)->entry.prev=(head)->prev; \
		(head)->prev->next=&(var)->entry; \
		(head)->prev=&(var)->entry; \
	} else { \
		(var)->entry.next=&(var)->entry; \
		(var)->entry.prev=&(var)->entry; \
		(head)=&(var)->entry; \
	} \
}

#define XLIST_ADDFIRST(entry,var,head) {\
	XLIST_ADD(entry,var,head); \
	(head)=&(var)->entry; \
}

#define XLIST_ADDLAST XLIST_ADD

#define XLIST_DELETE(entry,var,head) { \
	if (head) { \
		if (&(var)->entry==(head)) (head)=(var)->entry.next; \
		if (&(var)->entry==(head)) (head)=0; \
		else { \
			(var)->entry.next->prev=(var)->entry.prev; \
			(var)->entry.prev->next=(var)->entry.next; \
		} \
	} \
}

#endif /* QUEUE_H_ */
