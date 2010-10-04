/*
 * /phobos/bin/init/init.cpp
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright �AST 2009. Written by Artemy Lebedev.
 */

#include <sys.h>
phbSource("$Id$");

int myGlobalBSS, myGlobalInit = 237;

int
Main(GApp *app)
{
	GProcess *proc = app->GetProcess();
	CString str(GETSTR(proc, GProcess::GetName));
	str += ' ';
	str += GETSTR(proc, GProcess::GetName);
	str += '\n';
	GStream *out = app->GetStream("output");
	GTime *time = app->GetTime();

	GStream *in = app->GetStream("input");

	int rc;
	u8 c;
	while ((rc = in->Read(&c, 1)) >= 0) {
		if (rc) {
			CString s;
			s.Format("Character read: 0x%02lx ('%c')\n", (u32)c, c);
			out->Write((u8 *)s.GetBuffer(), s.GetLength());
		} else {
			app->Sleep(time->MS(10));
		}
	}

	return proc->GetPID();//temp
}
