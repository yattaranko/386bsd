/*
 * Copyright (c) 1994 William F. Jolitz.
 * 386BSD Copyright Restrictions Apply. All Other Rights Reserved.
 *
 * $Id: asm.h,v 1.1 94/10/19 17:39:54 bill Exp $
 * Assembly macros.
 */

#define	ENTRY(name) \
	.globl name; .align 4;  name:
#define	ALTENTRY(name) \
	.globl name; name:

