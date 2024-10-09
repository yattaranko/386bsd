/*-
 * Copyright (c) 1990 The Regents of the University of California.
 * All rights reserved.
 *
 * This code is derived from software contributed to Berkeley by
 * William Jolitz.
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
 * $Id: boot.c,v 1.1 94/10/20 16:45:28 root Exp $
 */

#include "sys/param.h"
#include "sys/reboot.h"
#include <a.out.h>
#include "saio.h"
#include "disklabel.h"
#include "ufs_dinode.h"

/*
 * Boot program, loaded by boot block from remaing 7.5K of boot area.
 * Sifts through disklabel and attempts to load an program image of
 * a standalone program off the disk. If keyboard is hit during load,
 * or if an error is encounter, try alternate files.
 */

char *dfiles[] = { ".askname", ".single", ".dfltroot",".kdb",
	 /* ".diagnostic", ".debug", */ 0};
int dflags[] = {RB_ASKNAME, RB_SINGLE, RB_DFLTROOT, RB_KDB};

char *files[] = { "386bsd", "386bsd.alt", "386bsd.old", "boot" , 0};
int	retry = 0;
extern struct disklabel disklabel;
extern	int bootdev, cyloffset;
static unsigned char *biosparams = (unsigned char *) 0x9ff00; /* XXX */

static int kurukuru_state = 0;
static const char kk_char[] = "-\\|/";
static char* CRT = (char*)0xb8000;

typedef int (*entry_point)(int, int, int);

#define	GAP_LOW		0x90000
#define	GAP_HIGH	0x100000

extern void printf(const char *fmt, ...);
extern int namei(char* s);
extern void wait(int n);
extern int iread(struct dinode* dip, int off, char* p, int sz);
extern void bzero(char* base, int cnt);
extern void bcopy(char* src, char* dst, int cnt);
extern void fetchi(int i, struct dinode* dip);
static void copyunix(int io, int howto, int cyloff);


void kurukuru()
{
	*CRT = kk_char[kurukuru_state];
	*(CRT + 1) = 0x07;
	kurukuru_state = (kurukuru_state + 1) % 4;
}

/*
 * Boot program... loads /boot out of filesystem indicated by arguements.
 * We assume an autoboot unless we detect a misconfiguration.
 */

/* main(dev, unit, off) */
void _main(int dev, int unit, int off)
{
	register struct disklabel *lp;
	register int io;
	register char **bootfile;
	int howto = 0;
	extern int scsisn; /* XXX */

	/* are we a disk, if so look at disklabel and do things */
	lp = &disklabel;
kurukuru();
#if	DEBUG > 0
	printf("cyl %x %x hd %x sect %x ",
		biosparams[0], biosparams[1], biosparams[2], biosparams[0xe]);
	printf("dev %x unit %x off %d\n", dev, unit, off);
#endif

	if (lp->d_magic == DISKMAGIC) {
	    /*
	     * Synthesize bootdev from dev, unit, type and partition
	     * information from the block 0 bootstrap.
	     * It's dirty work, but someone's got to do it.
	     * This will be used by the filesystem primatives, and
	     * drivers. Ultimately, opendev will be created corresponding
	     * to which drive to pass to top level bootstrap.
	     */
	    for (io = 0; io < lp->d_npartitions; io++) {
		int sn;

		if (lp->d_partitions[io].p_size == 0)
			continue;
		if (lp->d_type == DTYPE_SCSI)
			sn = off;
		else
			sn = off * lp->d_secpercyl;
		if (lp->d_partitions[io].p_offset == sn)
			break;
	    }

	    if (io == lp->d_npartitions) goto screwed;
            cyloffset = off;
	} else {
screwed:
		/* probably a bad or non-existant disklabel */
		io = 0 ;
		howto |= RB_SINGLE|RB_ASKNAME ;
	}

	/* construct bootdev */
	/* currently, PC has no way of booting off alternate controllers */
	bootdev = MAKEBOOTDEV(/*i_dev*/ dev, /*i_adapt*/0, /*i_ctlr*/0,
	    unit, /*i_part*/io, /*i_fs*/ 1);
kurukuru();

	/* first, probe for files associated with special flags quietly */
	bootfile = dfiles;
	for (;;) {
		io = namei(*bootfile);
		if (io > 2) {
			howto |= dflags[bootfile - dfiles];
#if	DEBUG > 0
		printf("howto %x ", howto);
#endif
		}

		if(*++bootfile == 0)
			break;
	}
kurukuru();

	bootfile = files;
	for (;;) {
		io = namei(*bootfile);
		if (io > 2)
			copyunix(io, howto, off);
		else
			printf("File not found");

		printf(" - didn't load %s, ", *bootfile);
		if (*++bootfile == 0)
			bootfile = files;
		printf("will try %s\n", *bootfile);

		wait(1<<((retry++) + 10));
	}
kurukuru();
}

/*ARGSUSED*/
/* copyunix(io, howto, cyloff)
	register io; */
void copyunix(int io, int howto, int cyloff)
{
	Elf32_Phdr phdr;
	Elf32_Off phoff;
	Elf32_Half phnum;
	Elf32_Word fsize, msize, lsize;
	Elf32_Addr entry;
	int l;

	struct exec x;
	int i;
	char *addr;
	struct dinode fil;
	int off, baseamt, extamt;

	fetchi(io, &fil);
#if	DEBUG > 1
printf("mode %o ", fil.di_mode);
#endif

	i = iread(&fil, 0,  (char *)&x, sizeof(x));
	if (i != sizeof(x) || !IS_ELF(x) || x.e_type != ET_EXEC)
	{
		printf("File is not an ELF executable format");
		return;
	}
	phnum = x.e_phnum;
	entry = 0xFFFFF & x.e_entry;
kurukuru();

	for (l = 0, phoff = (int)x.e_phoff; l < phnum; l++, phoff += sizeof(Elf32_Phdr))
	{
		// プログラムヘッダを読み込む
		i = iread(&fil, phoff,  (char *)&phdr, sizeof(Elf32_Phdr));
		if (i != sizeof(Elf32_Phdr))
		{
			printf("Cannot read program header[%d]\n", l);
			goto shread;
		}

		switch (phdr.p_type)
		{
		case PT_LOAD:
			addr = (char*)(0x00FFFFFF & phdr.p_paddr);	// アドレスの下位24ビットをマスクして代入する
			off = (int)phdr.p_offset;
			fsize = phdr.p_filesz;
			msize = phdr.p_memsz;

			if ((Elf32_Addr)(addr + fsize) < 0x1000)
			{
				// 0x0000～0x1000の範囲は読み飛ばす
				break;
			}

			/* 読み込むセグメントの上限がGAPの下限(0x90000)より小さいか、
			   セグメントの下限がGAPの上限(0x100000)より大きい場合 */
			if (((Elf32_Word)addr > GAP_LOW) || ((Elf32_Word)addr + msize) <= GAP_LOW)
			{
				baseamt = ((Elf32_Word)addr > GAP_LOW) ? GAP_HIGH - GAP_LOW : 0;
				// メモリに読み込み
				i = iread(&fil, off, addr + baseamt, fsize);
				if (i != fsize)
				{
					goto shread;
				}

				// メモリサイズがファイルサイズより大きいときはfsize～msizeを0クリア(.bss初期化)
				if (fsize < msize)
				{
					bzero((char*)(addr + fsize + baseamt), msize - fsize);
				}
			}

			/* 読み込むセグメントの下限がGAPの下限(0x90000)より小さく、
			   セグメントの上限がGAPの下限(0x90000)より大きい場合(※※ 未実装 ※※) */
/*
			if (((Elf32_Word)addr <= GAP_LOW) && (((Elf32_Word)addr + msize) > GAP_LOW))
			{
				lsize = GAP_LOW - (Elf32_Word)addr;
				// メモリに読み込み
				i = iread(&fil, off, addr, fsize);
				if (i != fsize)
				{
					goto shread;
				}

				// メモリサイズがファイルサイズより大きいときはfsize～msizeを0クリア(.bss初期化)
				if (fsize < msize)
				{
					bzero((char*)(addr + fsize), msize - fsize);
				}
			}
*/

kurukuru();
			break;
		default:
			break;
		}

	}

#if 0
	/* XXX: read syms and strings */
	{
		io = namei(".config");
		if (io > 2) {
			("cfg ");
			/*addr += 4096; (int)addr &= ~(4096-1);*/
			fetchi(io, &fil);
			(void)iread(&fil, 0, addr, 4096);
		}
	}
#endif

//printf("bootdev = %x\n", bootdev);
	bcopy((char*)0x9ff00, (char*)0x300, 0x20); /* XXX */
	i = ((entry_point)entry)(howto, bootdev, GAP_LOW);

	if (i)
		printf("Program exits with %d", i) ; 
kurukuru();
	return;


//-------------------------------------------------------------------

	i = iread(&fil, 0,  (char *)&x, sizeof x);
	off = sizeof x;
	if (i != sizeof x || x.a_magic != 0413) {
		printf("File is not an executable format");
		return;
	}

	/* check if image loaded will be larger than main memory */
	/* if (roundup(x.a_text, 4096) + x.a_data + x.a_bss > ???) {
		printf("Program larger than memory");
		return;
	} */
	addr = 0;
	off = 4096;

	/* check if program instruction contents larger than "low" RAM" */
	if ((unsigned)addr <= GAP_LOW && (unsigned)addr + x.a_text > GAP_LOW) {
#if	DEBUG > 1
		printf("File text split in two");
#endif
		baseamt = GAP_LOW - (unsigned)addr;
	} else
		baseamt = x.a_text;

#if	DEBUG > 1
	printf("o %x a %x s %x ", off, addr, baseamt);
#endif
	if (iread(&fil, off, addr, baseamt) != baseamt)
		goto shread;
	off += baseamt;
	addr += baseamt;

	if (baseamt != x.a_text) {
		addr = (char *)GAP_HIGH;
		extamt = x.a_text - baseamt;
#if	DEBUG > 1
		printf("o %x a %x s %x ", off, addr, extamt);
#endif
		if (iread(&fil, off, addr, extamt) != extamt)
			goto shread;
		off += extamt;
		addr += extamt;
	}

	addr = (char *)x.a_text;
	while ((int)addr & (NBPG-1))
		*addr++ = 0;
	
	/* check if program data contents larger than "low" RAM" */
	if ((unsigned)addr <= GAP_LOW && (unsigned)addr + x.a_data > GAP_LOW) {
#if	DEBUG > 1
		printf("File data split in two");
#endif
		baseamt = GAP_LOW - (int)addr;
	} else
		baseamt = x.a_data;

#if	DEBUG > 1
	printf("o %x a %x s %x ", off, addr, baseamt);
#endif
	if (iread(&fil, off, addr, baseamt) != baseamt)
		goto shread;
	off += baseamt;
	addr += baseamt;

	if (baseamt != x.a_data) {
		addr = (char *)GAP_HIGH;
		extamt = x.a_data - baseamt;
#if	DEBUG > 1
		printf("o %x a %x s %x ", off, addr, extamt);
#endif
		if (iread(&fil, off, addr, extamt) != extamt)
			goto shread;
		off += extamt;
		addr += extamt;
	}

#if	DEBUG > 1
	printf("o %x a %x bss %x ", off, addr, x.a_bss);
#endif
	if ((unsigned)addr <= GAP_LOW && (unsigned)addr + x.a_bss > GAP_LOW) {
#if	DEBUG > 1
		printf("File BSS split in two");
#endif
		baseamt = GAP_LOW - (int)addr;
		bzero(addr, baseamt);
		addr = (char *)GAP_HIGH;
		extamt = x.a_bss - baseamt;
		bzero(addr, extamt);
		addr += extamt;
	} else {
		bzero(addr, x.a_bss);
		addr += x.a_bss;
	}
/* XXX: read syms and strings */
{
	io = namei(".config");
	if (io > 2) {
printf("cfg ");
/*addr += 4096; (int)addr &= ~(4096-1);*/
		fetchi(io, &fil);
		(void)iread(&fil, 0, addr, 4096);
	}
}

	/* mask high order bits corresponding to relocated system base */
	x.a_entry &= ~0xfff00000;

	/*if (scankbd()) {
		printf("Operator abort");
		kbdreset();
		return;
	}*/

#if	DEBUG > 0
	printf("entry %x [%x]\n", x.a_entry, *(int *) x.a_entry);
#endif
	/* bcopy(0x9ff00, 0x300, 0x20); */ /* XXX */
	bcopy((char*)0x9ff00, (char*)0x300, 0x20); /* XXX */
	i = ((entry_point)(x.a_entry))(howto, bootdev, GAP_LOW);

	if (i)
		printf("Program exits with %d", i) ; 
	return;
shread:
	printf("Read of program is incomplete");
	return;
}
