/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * Don Ahn.
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
 *	@(#)fd.c	7.3 (Berkeley) 5/25/91
 */

/****************************************************************************/
/*                        standalone fd driver                               */
/****************************************************************************/
#include "sys/param.h"
#include "disklabel.h"
#include "fdreg.h"
#include "isa_stdports.h"
#include "nec765.h"
#include "saio.h"
#include "machine/inline/io.h"

#define NUMRETRY 10
#define NFD 2
#define FDBLK 512

extern struct disklabel disklabel;

struct fd_type {
	int	sectrac;		/* sectors per track         */
	int	secsize;		/* size code for sectors     */
	int	datalen;		/* data len when secsize = 0 */
	int	gap;			/* gap len between sectors   */
	int	tracks;			/* total num of tracks       */
	int	size;			/* size of disk in sectors   */
	int	steptrac;		/* steps per cylinder        */
	int	trans;			/* transfer speed code       */
};

struct fd_type fd_types[] = {
 	{ 18,2,0xFF,0x1B,80,2880,1,0 },	/* 1.44 meg HD 3.5in floppy    */
	{ 15,2,0xFF,0x1B,80,2400,1,0 },	/* 1.2 meg HD floppy           */
};


/* state needed for current transfer */
static int probetype = 0;
static int fd_type = 0;
static int fd_retry = 0;
static int fd_status[7] = {0, 0, 0, 0, 0, 0, 0};

static const int FD1_MSR = IO_FD1 + fdsts;	/* floppy disk status address */

/* Make sure DMA buffer doesn't cross 64k boundary */
static char bounce[FDBLK];

extern void printf(const char *fmt, ...);
extern void bcopy(char* src, char* dst, int cnt);
extern int fdio(int func, int unit, int blknum, char* address);
static int waitseek(int cyl);
static int in_fdc();
static void waitio();
static void out_fdc(int x);
static int fd_dma(int read, int addr, int nbytes);

/****************************************************************************/
/*                               fdstrategy                                 */
/****************************************************************************/
int fdstrategy(struct iob* io, int func)
{
	char *address;
	long nblocks,blknum;
 	int unit, iosize;

#if	DEBUG > 2
printf("fdstrat ");
#endif
	unit = io->i_unit;

	/*
	 * Set up block calculations.
	 */
    iosize = io->i_cc / FDBLK;		// 1セクタずつ読み込むためにセクタ数を計算
	blknum = (unsigned long) io->i_bn * DEV_BSIZE / FDBLK;
 	nblocks = fd_types[fd_type].size /*  disklabel.d_secperunit */;

	if ((blknum + iosize > nblocks) || blknum < 0) {
#if DEBUG > 8
		printf("bn = %d; sectors = %d; type = %d; fssize = %d ",
			blknum, iosize, fd_type, nblocks);
        printf("fdstrategy - I/O out of filesystem boundaries\n");
#endif
		return(-1);
	}

	address = (char *)io->i_ma;
	while (iosize > 0) {
		if (fdio(func, unit, blknum, address))
			return(-1);
		iosize--;
		blknum++;
		address += FDBLK;
	}
	return(io->i_cc);
}

static int ccyl = -1;

int fdio(int func, int unit, int blknum, char* address)
{
	int i,j, cyl, sectrac, sec, head, numretry;
	struct fd_type *ft;
/*printf("fdio ");*/
 	ft = &fd_types[fd_type];

 	sectrac = ft->sectrac;
	cyl = blknum / (sectrac * 2);
	numretry = NUMRETRY;

	if (func == F_WRITE) {
		bcopy(address, bounce, FDBLK);
	}

retry:
#if 0
	if (ccyl != cyl) {
		out_fdc(15);		/* Seek function */
		out_fdc(unit);		/* Drive number */
		out_fdc(cyl);

		waitio();
	}
	out_fdc(0x8);
	i = in_fdc();
	j = in_fdc();
	if (!(i & 0x20) || (cyl != j)) {
		numretry--;
		ccyl = j;
		if (numretry) goto retry;

		printf("Seek error %d, req = %d, at = %d\n",i,cyl,j);
		printf("unit %d, type %d, sectrac %d, blknum %d\n", unit,fd_type,sectrac,blknum);

		return -1;
	}
#endif
	ccyl = cyl;

	/* set up transfer */
	sec = blknum %  (sectrac * 2) /*disklabel.d_secpercyl*/;
	head = sec / sectrac;
	sec = sec % sectrac + 1;
#ifdef FDDEBUG
	printf("sec %d hd %d cyl %d ", sec, head, cyl);
#endif
	fd_dma(func == F_READ, (int)bounce, FDBLK);

	if (func == F_READ) {
		out_fdc(0xE6);			/* READ */
	} else {
		out_fdc(0xC5);			/* WRITE */
	}
	out_fdc(head << 2 | unit);	/* head & unit */
	out_fdc(cyl);				/* track */
	out_fdc(head);				/* head */
	out_fdc(sec);				/* sector */
	out_fdc(ft->secsize);		/* sector size */
	out_fdc(sectrac);			/* sectors/track */
	out_fdc(ft->gap);			/* gap size */
	out_fdc(ft->datalen);		/* data length */

	waitio();

	for(i = 0; i < 7; i++) {
		fd_status[i] = in_fdc();
	}

//	if (fd_status[0] & 0xF8) {
	if (fd_status[0] & 0xD8) {	// QEMUの場合正常終了時でもBIT5が立つことがあるのでこのビットは読まない
		numretry--;

		if (!probetype) {
			printf("FD err %lx %lx %lx %lx %lx %lx %lx\n",
			fd_status[0], fd_status[1], fd_status[2], fd_status[3],
			fd_status[4], fd_status[5], fd_status[6] );
		}

		if (numretry) {
			goto retry;
		}
		return -1;
	}
	if (func == F_READ) {
		bcopy(bounce, address, FDBLK);
	}
	return 0;
}

/****************************************************************************/
/*                             fdc in/out                                   */
/****************************************************************************/
int
in_fdc()
{
	int i;
	while ((i = inb(FD1_MSR) & 192) != 192) {
		if (i == 128) {
			return -1;
		}
	}
	return inb(0x3f5);
}

void set_intr()
{
	/* initialize 8259's */
	outb(0x20,0x11);
	outb(0x21,32);
	outb(0x21,4);
	outb(0x21,1);
	outb(0x21,0x0f); /* turn on int 6 */
}

void waitio()
{
	char c;

	do {
		outb(0x20, 0xc); /* read polled interrupt */
	} while ((c = inb(0x20)) & 0x7f != 6); /* wait for int */
	outb(0x20, 0x20);
}

void out_fdc(int x)
{
	int r;

	do {
		r = (inb(FD1_MSR) & 192);
		if (r == 128) {
			break;
		}
	} while (1);

	outb(0x3f5, x & 0xFF);
}

/****************************************************************************/
/*                           fdopen/fdclose                                 */
/****************************************************************************/
int fdopen(register struct iob* io)
{
	int unit, type, i;
	struct fd_type *ft;
	char buf[512];

	unit = io->i_unit;
	/* type = io->i_part; */
	io->i_boff = 0;		/* no disklabels -- tar/dump wont work */
/*("fdopen %d %d ", unit, type);*/
#ifdef FDDEBUG
	printf("fdopen %d %d ", unit, type);
#endif
 	ft = &fd_types[0];
//	fd_drive = unit;

	set_intr(); /* init intr cont */

	/* Try a reset, keep motor on */
	outb(0x3f2,0);
	for(i=0; i < 100000; i++);
	outb(0x3f2,unit | (unit  ? 32 : 16) );
	for(i=0; i < 100000; i++);
	outb(0x3f2,unit | 0xC | (unit  ? 32 : 16) );
	outb(0x3f7,ft->trans);
//	fd_motor = 1;

	waitio();

	out_fdc(3); /* specify command */
	out_fdc(0xDF);
	out_fdc(2);

	out_fdc(7);	/* Recalibrate Function */
	out_fdc(unit);

	waitio();
	probetype = 1;
	for (fd_type = 0; fd_type < sizeof(fd_types)/sizeof(fd_types[0]);
		fd_type++, ft++) {
		/*for(i=0; i < 100000; i++);
		outb(0x3f7,ft->trans);
		for(i=0; i < 100000; i++);*/
		if (fdio(F_READ, unit, ft->sectrac-1, buf) >= 0){
			probetype = 0;
			return(0);
		}
	}
	printf("failed fdopen");
	return(-1);
}

/*
 * set up DMA read/write operation and virtual address addr for nbytes
 */
int fd_dma(int read, int addr, int nbytes)
{
	outb(0x0A, 6);					// mask DMA channel 2
	outb(0xd8, 0xff);				// reset master flip-flop

	outb(0xb, read ? 0x56 : 0x5a);	// select read/write

	/* Send start address */
	outb(0x4, addr & 0xFF);
	outb(0x4, (addr >> 8) & 0xFF);
	outb(0x81, (addr >> 16) & 0xFF);

	outb(0xd8, 0xff);				// reset master flip-flop

	/* Send count */
	nbytes--;
	outb(0x5, nbytes & 0xFF);
	outb(0x5, (nbytes >> 8) & 0xFF);

	outb(0x80, 0);					// external page register = 0
	outb(0x0A, 2);					// unmask DMA channel 2
}
