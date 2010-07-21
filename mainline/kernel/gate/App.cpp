/*
 * /phobos/kernel/gate/App.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <gate/gate.h>

DEFINE_GCLASS(GApp);

GApp::GApp()
{
}

GApp::~GApp()
{

}

PM::pid_t
GApp::GetPID()
{
	return proc->GetID();
}
