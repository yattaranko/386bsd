/*
 * Copyright (c) 1992 William F. Jolitz, TeleMuse
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
 *	This software is a component of "386BSD" developed by 
	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE THIS WORK.
 *
 * FOR USERS WHO WISH TO UNDERSTAND THE 386BSD SYSTEM DEVELOPED
 * BY WILLIAM F. JOLITZ, WE RECOMMEND THE USER STUDY WRITTEN 
 * REFERENCES SUCH AS THE  "PORTING UNIX TO THE 386" SERIES 
 * (BEGINNING JANUARY 1991 "DR. DOBBS JOURNAL", USA AND BEGINNING 
 * JUNE 1991 "UNIX MAGAZIN", GERMANY) BY WILLIAM F. JOLITZ AND 
 * LYNNE GREER JOLITZ, AS WELL AS OTHER BOOKS ON UNIX AND THE 
 * ON-LINE 386BSD USER MANUAL BEFORE USE. A BOOK DISCUSSING THE INTERNALS 
 * OF 386BSD ENTITLED "386BSD FROM THE INSIDE OUT" WILL BE AVAILABLE LATE 1992.
 *
 * THIS SOFTWARE IS PROVIDED BY THE DEVELOPER ``AS IS'' AND
 * ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO, THE
 * IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE
 * ARE DISCLAIMED.  IN NO EVENT SHALL THE DEVELOPER BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF
 * SUCH DAMAGE.
 *
 * Routines to sift through a BSD fast filesystem. -wfj
 * $Id: ufsboot_fs.c,v 1.1 94/10/20 10:56:49 root Exp $
 */

#include "sys/param.h"
#include "ufs.h"
#include "ufs_dir.h"
#include "ufs_dinode.h"
#include "saio.h"

int bdev;
char superb[SBSIZE], abuf[MAXBSIZE];
struct fs *fs;

extern void exit(int status);
extern void printf(const char *fmt, ...);
extern void bcopy(char* src, char* dst, int cnt);
extern int strcmp(const char *s1, const char *s2);
extern int bread(int bdev, int off, char* addr, int sz);
extern int bmap(struct dinode* dip, daddr_t bn, daddr_t* bnp);
extern void fetchi(int i, struct dinode* dip);
extern int iread(struct dinode* dip, int off, char* p, int sz);
static int ilookup(struct dinode* dip, char* s);

/*
 * Translate name to inode number.
 */
int namei(char* s)
{
	int ino;
	struct dinode rd;

#if	DEBUG > 1
	printf("namei %s\n", s);
#endif
	if (!fs) {
		bread(bdev, SBOFF/DEV_BSIZE, superb, SBSIZE);
		fs = (struct fs *)superb;
	}
	fetchi(2, &rd); 
	return(ilookup(&rd, s));
}

/*
 * look for a file in this inode.
 */
int ilookup(struct dinode* dip, char* s)
{
	struct direct dirent;
	int off;

#if	DEBUG > 2
	printf("ilookup %x %s %d\n", dip, s, dip->di_size);
#endif
	off = 0;
	do {
		iread(dip, off, (char *)&dirent, sizeof(struct direct));
		off += dirent.d_reclen;
#if	DEBUG > 8
		printf("%s ", dirent.d_name);
#endif
		if (strcmp (dirent.d_name, s) == 0)
			return (dirent.d_ino);
	} while (off < dip->di_size && dirent.d_reclen);
	return (0);
}

/*
 * Extract an inode and return it.
 */
void fetchi(int i, struct dinode* dip)
{

#if	DEBUG > 2
	printf("fetchi %d %x\n", i, dip);
#endif
	bread(bdev, fsbtodb(fs, itod(fs, i)), abuf, fs->fs_bsize);
	bcopy (abuf + itoo(fs,i) * sizeof(struct dinode),
		(char*)dip, sizeof(struct dinode));
#if	DEBUG > 8
	printf("mode %o link %d uid %d gid %d size %d [ ",
	dip->di_mode, dip->di_nlink, dip->di_uid, dip->di_gid, dip->di_size);
	for (i=0; i < NDADDR; i++)
		printf("%d ", dip->di_db[i]);
	printf("] (");
	for (i=0; i < NIADDR; i++)
		printf("%d ", dip->di_ib[i]);
	printf(")\n");
#endif
}

/*
 * Read data contents of an inode
 */
int iread(struct dinode* dip, int off, char* p, int sz)
{
	daddr_t physblock;
	int va = sz;
	char *op, *pp;

#if	DEBUG > 2
	printf("iread %x %d %x %d\n", dip, off, p, sz);
#endif
	while (sz > 0) {
		int lbn, bs, o;

		lbn = lblkno(fs, off);
		bs = dblksize(fs, dip, lbn);
		o = blkoff(fs, off);

		/* logical to physical translation */
		bmap(dip, lbn, &physblock);

		/* if sz larger than blksize, i/o direct,
		   otherwise to local buffer */
		if (o == 0 && bs <= sz)
			bread(bdev, physblock, p, bs);
		else {
			bread(bdev, physblock, abuf, bs);
			bs -= o;
			bs = bs > sz ? sz : bs;
			bcopy(abuf + o, p, bs);
		}
#if	DEBUG > 8
		printf("bs %d sz %d", bs, sz);
#endif
		sz -= bs;
		p += bs;
		off += bs;
		if (bs==0) break;
	}
	return(va);
}

void _stop(char* s)
{
	printf("Failed: %s\n", s);
	exit(0);
}
