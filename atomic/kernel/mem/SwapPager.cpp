/*
 * /kernel/mem/SwapPager.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c) 2011, Artyom Lebedev <artyom.lebedev@gmail.com>
 * All rights reserved.
 * See COPYING file for copyright details.
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
