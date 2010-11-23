/*
 * /phobos/lib/user/mem.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright (c)AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

USlabAllocator::USlabAllocator(GApp *app) : client(app), alloc(&client)
{
	app->AddRef();
	this->app = app;
}

USlabAllocator::~USlabAllocator()
{
	app->Release();
}

void *
USlabAllocator::malloc(u32 size)
{
	return alloc.malloc(size);
}

void
USlabAllocator::mfree(void *p)
{
	alloc.mfree(p);
}

void *
USlabAllocator::mrealloc(void *p, u32 size)
{
	return alloc.mrealloc(p, size);
}

/* USlabAllocator::SSlabClient class */

USlabAllocator::SSlabClient::SSlabClient(GApp *app)
{
	app->AddRef();
	this->app = app;
}

USlabAllocator::SSlabClient::~SSlabClient()
{
	app->Release();
}

void *
USlabAllocator::SSlabClient::malloc(u32 size)
{
	return app->AllocateHeap(size);
}

void
USlabAllocator::SSlabClient::mfree(void *p)
{
	app->FreeHeap(p);
}

void
USlabAllocator::SSlabClient::Lock()
{
	//notimpl
}

void
USlabAllocator::SSlabClient::Unlock()
{
	//notimpl
}

UMemAllocator::UMemAllocator()
{
	alloc = uLib->GetMemAllocator();
}

void *
operator new(size_t size)
{
	return uLib->GetMemAllocator()->malloc(size);
}

void *
operator new(size_t size, const char *className, const char *fileName, int line)
{
	return uLib->GetMemAllocator()->malloc(size);
}

void
operator delete(void *p)
{
	uLib->GetMemAllocator()->mfree(p);
}

void
operator delete[](void *p)
{
	uLib->GetMemAllocator()->mfree(p);
}
