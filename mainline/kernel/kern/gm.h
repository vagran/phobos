/*
 * /phobos/gm.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef GM_H_
#define GM_H_
#include <sys.h>
phbSource("$Id$");

class GM : public Object {
private:
public:
	GM();

	int InitCPU(); /* called by each CPU */
};

extern GM *gm;

#endif /* GM_H_ */
