/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz and Don Ahn.
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
 *	@(#)isa_pit.c	7.2 (Berkeley) 5/12/91
 */

/*
 * Primitive clock interrupt routines.
 */
/* standard AT configuration: (will always be configured if loaded) */
static	char *pit_config =
	"clock (0 0).	# process timeslice clock $Revision$";

#include "sys/param.h"
#include "sys/time.h"
#include "sys/errno.h"
#include "tzfile.h"
#include "kernel.h"
#include "malloc.h"
#include "modconfig.h"

#include "machine/cpu.h"
#include "machine/pcb.h"
#include "machine/inline/io.h"

#include "machine/icu.h"
#include "prototypes.h"
#include "isa_stdports.h"
#include "isa_irq.h"
#include "rtc.h"
#include "isa_driver.h"

#define DAYST 119
#define DAYEN 303

#define	TIMER_FREQ	1193182	/* XXX - should be in isa.h */
#define TIMER_DIV(x) ((TIMER_FREQ + (x) / 2) / (x))

/*
 * Macros for specifying values to be written into a mode register.
 */
#define	TIMER_CNTR0		(IO_TIMER1 + 0)	/* timer 0 counter port */
#define	TIMER_CNTR1		(IO_TIMER1 + 1)	/* timer 1 counter port */
#define	TIMER_CNTR2		(IO_TIMER1 + 2)	/* timer 2 counter port */
#define	TIMER_MODE		(IO_TIMER1 + 3)	/* timer mode port */
#define	TIMER_SEL0		0x00			/* select counter 0 */
#define	TIMER_SEL1		0x40			/* select counter 1 */
#define	TIMER_SEL2		0x80			/* select counter 2 */
#define	TIMER_INTTC		0x00			/* mode 0, intr on terminal cnt */
#define	TIMER_ONESHOT	0x02			/* mode 1, one shot */
#define	TIMER_RATEGEN	0x04			/* mode 2, rate generator */
#define	TIMER_SQWAVE	0x06			/* mode 3, square wave */
#define	TIMER_SWSTROBE	0x08			/* mode 4, s/w triggered strobe */
#define	TIMER_HWSTROBE	0x0a			/* mode 5, h/w triggered strobe */
#define	TIMER_LATCH		0x00			/* latch counter for reading */
#define	TIMER_LSB		0x10			/* r/w counter LSB */
#define	TIMER_MSB		0x20			/* r/w counter MSB */
#define	TIMER_16BIT		0x30			/* r/w counter 16 bits, LSB first */
#define	TIMER_BCD		0x01			/* count in BCD */

static int ena;
/*
 * Wire clock interrupt in.
 */
void enablertclock(void)
{
	ena = 1;
}

static int hi = 0xffff;
static int pitprobe(struct isa_device *dvp);
static void pitattach(struct isa_device *dvp);
static void pitintr(struct intrframe f);
struct	isa_driver pitdriver = {
	pitprobe, pitattach, (void (*)(int))pitintr, "pit", &hi
};

/*
 * Probe routine - look device, otherwise set emulator bit
 */
static int
pitprobe(struct isa_device *dvp)
{
	return (1);
}

static void
pitattach(struct isa_device *dvp)
{
	printf("<Intel 8253/8254 PIT>");

	splx(0xf); /* XXX */
}

static void
pitintr(struct intrframe f)
{
	clockframe cf = {f.if_ppl, f.if_eip, f.if_cs};

	if (!ena)
	{
		return;
	}
	splhigh();
	hardclock(cf);
}

extern u_short getit(int unit, int timer);
unsigned long it_ticksperintr;

KERNEL_MODCONFIG(pit)
{
	char *cfg_string;
	
	/* find configuration string. */
	if (!config_scan(pit_config, &cfg_string)) 
	{
		return;
	}
	/* probe for hardware */
	new_isa_configure(&cfg_string, &pitdriver);

	/* initialize 8253 clock */
	it_ticksperintr = TIMER_DIV(hz);
	outb (TIMER_MODE , TIMER_SEL0 | TIMER_RATEGEN | TIMER_16BIT);
	outb (TIMER_CNTR0, it_ticksperintr & 0xff);
	outb (TIMER_CNTR0, it_ticksperintr >> 8);

	register int cnt;
	int tick;

	/*
	 * Find out how many loops we need to DELAY() a microsecond
	 */

	/* wait till overflow to insure no wrap around */
	do
	{
		tick = getit(0,0);
	}
	while (tick < getit(0,0));

	/* time a while loop */
	cnt = 1000;
	tick = getit(0,0);
	while (cnt-- > 0)
		;
	tick -= getit(0,0);

	/* scale to microseconds per 1000 "loops" */
	tick = (tick * 1000000) / TIMER_FREQ;
	loops_per_usec = (10000 / tick + 5) / 10;
}
