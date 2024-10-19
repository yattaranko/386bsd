/*-
 * Copyright (c) 1982, 1986 The Regents of the University of California.
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 * 1. Redistributions of source code must retain the above copyright
 *    notice, this list of conditions and the following disclaimer.
 * 2. Redistributions in binary form must reproduce the above copyright
 *    notice, this list of conditions and the following disclaimer in the
 *    documentation and/or other materials provided with the distribution.
 * 3. All advertising materials mentioning features or use of this software
 *    must display the following acknowledgement:
 *	This product includes software developed by the University of
 *	California, Berkeley and its contributors.
 * 4. Neither the name of the University nor the names of its contributors
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE REGENTS AND CONTRIBUTORS ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE REGENTS OR CONTRIBUTORS BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 *	@(#)exec.h	7.5 (Berkeley) 2/15/91
 */

#ifndef	_EXEC_H_
#define	_EXEC_H_

/* Header prepended to each a.out file. */
#if 0
struct exec
{
#if !defined(vax) && !defined(tahoe) && !defined(i386)
	unsigned short	a_mid;		/* machine ID */
	unsigned short	a_magic;	/* magic number */
#else
	long			a_magic;	/* magic number */
#endif
	unsigned long	a_text;		/* text segment size */
	unsigned long	a_data;		/* initialized data size */
	unsigned long	a_bss;		/* uninitialized data size */
	unsigned long	a_syms;		/* symbol table size */
	unsigned long	a_entry;	/* entry point */
	unsigned long	a_trsize;	/* text relocation size */
	unsigned long	a_drsize;	/* data relocation size */
};
#else
typedef struct
{
	long			a_magic;	/* magic number */
	unsigned long	a_text;		/* text segment size */
	unsigned long	a_data;		/* initialized data size */
	unsigned long	a_bss;		/* uninitialized data size */
	unsigned long	a_syms;		/* symbol table size */
	unsigned long	a_entry;	/* entry point */
	unsigned long	a_trsize;	/* text relocation size */
	unsigned long	a_drsize;	/* data relocation size */
} Aout_Ehdr;
#endif

#if 0
#define	a_machtype	a_mid	/* SUN compatibility */
#endif

/* a_magic */
#define	OMAGIC		0407	/* old impure format */
#define	NMAGIC		0410	/* read-only text */
#define	ZMAGIC		0413	/* demand load format */

#if 0
/* a_mid */
#define	MID_ZERO	0		/* unknown - implementation dependent */
#define	MID_SUN010	1		/* sun 68010/68020 binary */
#define	MID_SUN020	2		/* sun 68020-only binary */
#define	MID_HP200	200		/* hp200 (68010) BSD binary */
#define	MID_HP300	300		/* hp300 (68020+68881) BSD binary */
#define	MID_HPUX	0x20C	/* hp200/300 HP-UX binary */
#define	MID_HPUX800	0x20B   /* hp800 HP-UX binary */
#endif

#include "sys/elf32.h"

struct exec
{
	union
	{
		Aout_Ehdr	aout_ehdr;
		Elf32_Ehdr	elf32_ehdr;
	};
};

// aoutメンバーアクセス用マクロ
#define	a_magic		aout_ehdr.a_magic
#define	a_text		aout_ehdr.a_text
#define	a_data		aout_ehdr.a_data
#define	a_bss		aout_ehdr.a_bss
#define	a_syms		aout_ehdr.a_syms
#define	a_entry		aout_ehdr.a_entry
#define	a_trsize	aout_ehdr.a_trsize
#define	a_drsize	aout_ehdr.a_drsize

#define IS_AOUT(hdr)	((hdr).a_magic == ZMAGIC)

// elf32メンバーアクセス用マクロ
#define e_ident		elf32_ehdr.e_ident
#define e_type		elf32_ehdr.e_type
#define e_machine	elf32_ehdr.e_machine
#define e_version	elf32_ehdr.e_version
#define e_entry		elf32_ehdr.e_entry
#define e_phoff		elf32_ehdr.e_phoff
#define e_shoff		elf32_ehdr.e_shoff
#define e_flags		elf32_ehdr.e_flags
#define e_ehsize	elf32_ehdr.e_ehsize
#define e_phentsize	elf32_ehdr.e_phentsize
#define e_phnum		elf32_ehdr.e_phnum
#define e_shentsize	elf32_ehdr.e_shentsize
#define e_shnum		elf32_ehdr.e_shnum
#define e_shstrndx	elf32_ehdr.e_shstrndx

#define ELF_MAXPHNUM		128
#define ELF_MAXSHNUM		32768
#define ELF_MAXNOTESIZE		1024

#define	ELF_ROUND(a, b)		(((a) + (b) - 1) & ~((b) - 1))
#define	ELF_TRUNC(a, b)		((a) & ~((b) - 1))

#endif /* !_EXEC_H_ */
