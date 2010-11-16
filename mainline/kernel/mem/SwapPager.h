/*
 * /kernel/mem/SwapPager.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#ifndef SWAPPAGER_H_
#define SWAPPAGER_H_
#include <sys.h>
phbSource("$Id$");

class SwapPager : public MM::Pager {
private:
public:
	SwapPager(vsize_t size, Handle handle = 0);
	virtual ~SwapPager();

	virtual int HasPage(vaddr_t offset);
	virtual int GetPage(vaddr_t offset, MM::Page **ppg, int numPages = 1);
	virtual int PutPage(vaddr_t offset, MM::Page **ppg, int numPages = 1);
};

#endif /* SWAPPAGER_H_ */
