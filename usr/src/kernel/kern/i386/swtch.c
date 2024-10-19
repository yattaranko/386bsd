/*-
 * Copyright (c) 2024 Seiji Kato
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
 */

#include "sys/param.h"
#include "sys/errno.h"
#include "proc.h"
#include "prototypes.h"

#include "../obj/assym.S"

extern int cnt;
extern int whichqs;
extern int dma_active;

extern void splnone();
extern void printf(const char *fmt, ...);

void idle()
{
__asm__ volatile
("_idle:");

    while(1)
    {
        splnone();
        __asm__ volatile ("         \
            cli;                    \
            cmpl $0, whichqs;       \
            jne  swtch_selq;        \
            sti;                    \
        ");
/*
        if (dma_active != 0)
        {
            continue;
        }
*/
        __asm__ volatile ("hlt");
    }
}

void swtch()
{
    (*(&cnt + V_SWTCH))++;

    __asm__ volatile ("             \
        xorl    %eax, %eax;         \
        xchgl   %eax, curproc;      \
    	pushl	%eax;               \
    	pushl	%ebx;               \
	    pushl	%esi");

__asm__ volatile
("swtch_selq:");

    __asm__ volatile ("             \
        bsfl	whichqs, %eax;      \
    	jz      _idle;			    \
    ");

    __asm__ volatile ("             \
        btrl	%eax, whichqs;      \
        jnc		swtch_selq;         \
    ");

    __asm__ volatile ("             \
        cli;					    \
        movl	%eax, %ebx;		    \
        shll	$3, %eax;		    \
        addl	$qs, %eax;          \
        movl	%eax, %esi;         \
    ");

    __asm__ volatile ("             \
        movl	%c0(%%eax), %%ecx;  \
        movl	%c0(%%ecx), %%edx;  \
        movl	%%edx, %c0(%%eax);  \
        movl	%c1(%%ecx), %%eax;  \
        movl	%%eax, %c1(%%edx);  \
    	cmpl	%c0(%%ecx), %%esi;  \
    	je		1f;                 \
    	btsl	%%ebx, whichqs;     \
    " :: "i" (P_LINK), "i" (P_RLINK));

    __asm__ volatile ("             \
1:    	xorl	%%eax, %%eax;       \
    	movl	%%eax, %c0(%%ecx);  \
    	popl    %%esi;              \
	    popl    %%ebx;              \
    	popl    %%eax;              \
    " :: "i" (P_RLINK));

    __asm__ volatile ("             \
    	cmpl	%ecx, %eax;         \
	    je		1f;                 \
    ");

    __asm__ volatile ("             \
        ljmp	*(%c0)(%%ecx);      \
    ":: "i" (PMD_TSEL - 4));

    __asm__ volatile ("             \
1:	    movl	%eax, curproc;      \
        sti;                        \
    ");

}