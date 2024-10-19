/*
 * Copyright (c) 1989 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: prf.c,v 1.1 94/10/20 16:45:30 root Exp $
 * dummy interface functions.
 */

#include "sys/types.h"

extern void sput(u_char c);
extern void kbdreset();

int putchar(char c)
{
        if (c == '\n')
		sput('\r');
	sput(c);
	return(0);
}

void wait(int n) {
	int v;

	while(n-- /* && (v = scankbd()) == 0*/);
	if (v) kbdreset();
}
