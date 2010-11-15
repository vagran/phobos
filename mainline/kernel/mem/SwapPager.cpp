/*
 * /kernel/mem/SwapPager.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <mem/SwapPager.h>

SwapPager::SwapPager(vsize_t size, Handle handle) :
	MM::Pager(T_SWAP, size, handle)
{

}

SwapPager::~SwapPager()
{

}

int
SwapPager::HasPage(vaddr_t offset)
{
	//notimpl
	return 0;
}

int
SwapPager::GetPage(vaddr_t offset, MM::Page **ppg, int numPages)
{
	//notimpl
	return 0;
}

int
SwapPager::PutPage(vaddr_t offset, MM::Page **ppg, int numPages)
{
	//notimpl
	return 0;
}
