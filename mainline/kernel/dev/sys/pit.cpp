/*
 * /kernel/dev/sys/pit.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

#include <dev/sys/pit.h>

DefineDevFactory(PIT);

RegDevClass(PIT, "pit", Device::T_SPECIAL, "Programmable Interval Timer (8253/8254)");

PIT::PIT(Type type, u32 unit, u32 classID) : Device(type, unit, classID)
{

}
