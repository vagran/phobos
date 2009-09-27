/*
 * /kernel/sys/gcc.h
 * $Id$
 *
 * This file is a part of PhobOS operating system.
 * Copyright ©AST 2009. Written by Artemy Lebedev.
 */

#ifndef GCC_H_
#define GCC_H_
#include <sys.h>
phbSource("$Id$");

class CXA {
public:
	typedef struct {
		u32		n;
		void	(*func[])();
	} CallbackList;

private:
	typedef struct {
		void	(*func)(void *);
		void	*arg;
		void	*dso_handle;
	} Destructor;


public:
	static int ConstructStaticObjects();
	static int DestructStaticObjects();
};

#endif /* GCC_H_ */
