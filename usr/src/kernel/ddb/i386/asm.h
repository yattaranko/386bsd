/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: kernel.h,v 1.1 94/06/09 18:10:23 bill Exp Locker: bill $
 * Assembly macros.
 */

#define	ENTRY(name) \
	.globl name; .align 4;  name:
#define	ALTENTRY(name) \
	.globl name; name:

