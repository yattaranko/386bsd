/*
 * Copyright (c) 1982, 1986 Regents of the University of California.
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
 *	@(#)fs.h	7.12 (Berkeley) 5/8/91
 */

/*
 * Each disk drive contains some number of file systems.
 * A file system consists of a number of cylinder groups.
 * Each cylinder group has inodes and data.
 *
 * A file system is described by its super-block, which in turn
 * describes the cylinder groups.  The super-block is critical
 * data and is replicated in each cylinder group to protect against
 * catastrophic loss.  This is done at `newfs' time and the critical
 * super-block data does not change, so the copies need not be
 * referenced further unless disaster strikes.
 *
 * For file system fs, the offsets of the various blocks of interest
 * are given in the super block as:
 *	[fs->fs_sblkno]		Super-block
 *	[fs->fs_cblkno]		Cylinder group block
 *	[fs->fs_iblkno]		Inode blocks
 *	[fs->fs_dblkno]		Data blocks
 * The beginning of cylinder group cg in fs, is given by
 * the ``cgbase(fs, cg)'' macro.
 *
 * The first boot and super blocks are given in absolute disk addresses.
 * The byte-offset forms are preferred, as they don't imply a sector size.
 */
#define BBSIZE		8192
#define SBSIZE		8192
#define	BBOFF		((off_t)(0))
#define	SBOFF		((off_t)(BBOFF + BBSIZE))
#define	BBLOCK		((daddr_t)(0))
#define	SBLOCK		((daddr_t)(BBLOCK + BBSIZE / DEV_BSIZE))

/*
 * Addresses stored in inodes are capable of addressing fragments
 * of `blocks'. File system blocks of at most size MAXBSIZE can 
 * be optionally broken into 2, 4, or 8 pieces, each of which is
 * addressible; these pieces may be DEV_BSIZE, or some multiple of
 * a DEV_BSIZE unit.
 *
 * Large files consist of exclusively large data blocks.  To avoid
 * undue wasted disk space, the last data block of a small file may be
 * allocated as only as many fragments of a large block as are
 * necessary.  The file system format retains only a single pointer
 * to such a fragment, which is a piece of a single large block that
 * has been divided.  The size of such a fragment is determinable from
 * information in the inode, using the ``blksize(fs, ip, lbn)'' macro.
 *
 * The file system records space availability at the fragment level;
 * to determine block availability, aligned fragments are examined.
 *
 * The root inode is the root of the file system.
 * Inode 0 can't be used for normal purposes and
 * historically bad blocks were linked to inode 1,
 * thus the root inode is 2. (inode 1 is no longer used for
 * this purpose, however numerous dump tapes make this
 * assumption, so we are stuck with it)
 */
#define	ROOTINO		((ino_t)2)

/*
 * MINBSIZE is the smallest allowable block size.
 * In order to insure that it is possible to create files of size
 * 2^32 with only two levels of indirection, MINBSIZE is set to 4096.
 * MINBSIZE must be big enough to hold a cylinder group block,
 * thus changes to (struct cg) must keep its size within MINBSIZE.
 * Note that super blocks are always of size SBSIZE,
 * and that both SBSIZE and MAXBSIZE must be >= MINBSIZE.
 */
#define MINBSIZE	4096

/*
 * The path name on which the file system is mounted is maintained
 * in fs_fsmnt. MAXMNTLEN defines the amount of space allocated in 
 * the super block for this name.
 * The limit on the amount of summary information per file system
 * is defined by MAXCSBUFS. It is currently parameterized for a
 * maximum of two million cylinders.
 */
#define MAXMNTLEN 512
#define MAXCSBUFS 32

/*
 * There is a 128-byte region in the superblock reserved for in-core
 * pointers to summary information. Originally this included an array
 * of pointers to blocks of struct csum; now there are just two
 * pointers and the remaining space is padded with fs_ocsp[].
 *
 * NOCSPTRS determines the size of this padding. One pointer (fs_csp)
 * is taken away to point to a contiguous array of struct csum for
 * all cylinder groups; a second (fs_maxcluster) points to an array
 * of cluster sizes that is computed as cylinder groups are inspected.
 */
#define	NOCSPTRS	((128 / sizeof(void *)) - 2)

/*
 * The maximum number of snapshot nodes that can be associated
 * with each filesystem. This limit affects only the number of
 * snapshot files that can be recorded within the superblock so
 * that they can be found when the filesystem is mounted. However,
 * maintaining too many will slow the filesystem performance, so
 * having this limit is a good idea.
 */
#define FSMAXSNAP 20

/*
 * Per cylinder group information; summarized in blocks allocated
 * from first cylinder group data blocks.  These blocks have to be
 * read in from fs_csaddr (size fs_cssize) in addition to the
 * super block.
 *
 * N.B. sizeof(struct csum) must be a power of two in order for
 * the ``fs_cs'' macro to work (see below).
 */
struct csum {
	long	cs_ndir;	/* number of directories */
	long	cs_nbfree;	/* number of free blocks */
	long	cs_nifree;	/* number of free inodes */
	long	cs_nffree;	/* number of free frags */
};

struct csum_total {
	quad	cs_ndir;		/* number of directories */
	quad	cs_nbfree;		/* number of free blocks */
	quad	cs_nifree;		/* number of free inodes */
	quad	cs_nffree;		/* number of free frags */
	quad	cs_numclusters;	/* number of free clusters */
	quad	cs_spare[3];	/* future expansion */
};

/*
 * Super block for a file system.
 */
#define	FS_MAGIC		0x011954
#define	FSOKAY			0x7c269d38
#define FS_42INODEFMT	-1		/* 4.2BSD inode format */
#define FS_44INODEFMT	2		/* 4.4BSD inode format */

struct	fs
{
	struct fs*	fs_link;		/*    0:linked list of file systems */
	struct fs*	fs_rlink;		/*    4:used for incore super blocks */
	daddr_t	fs_sblkno;			/*    8:addr of super-block in filesys */
	daddr_t	fs_cblkno;			/*   12:offset of cyl-block in filesys */
	daddr_t	fs_iblkno;			/*   16:offset of inode-blocks in filesys */
	daddr_t	fs_dblkno;			/*   20:offset of first data after cg */
	long	fs_cgoffset;		/*   24:cylinder group offset in cylinder */
	long	fs_cgmask;			/*   28:used to calc mod fs_ntrak */
	time_t 	fs_old_time;   		/*   32:last time written */
	long	fs_size;			/*   36:number of blocks in fs */
	long	fs_dsize;			/*   40:number of data blocks in fs */
	long	fs_ncg;				/*   44:number of cylinder groups */
	long	fs_bsize;			/*   48:size of basic blocks in fs */
	long	fs_fsize;			/*   52:size of frag blocks in fs */
	long	fs_frag;			/*   56:number of frags in a block in fs */
/* these are configuration parameters */
	long	fs_minfree;			/*   60:minimum percentage of free blocks */
	long	fs_rotdelay;		/*   64:num of ms for optimal next block */
	long	fs_rps;				/*   68:disk revolutions per second */
/* these fields can be computed from the others */
	long	fs_bmask;			/*   72:``blkoff'' calc of blk offsets */
	long	fs_fmask;			/*   76:``fragoff'' calc of frag offsets */
	long	fs_bshift;			/*   80:``lblkno'' calc of logical blkno */
	long	fs_fshift;			/*   84:``numfrags'' calc number of frags */
/* these are configuration parameters */
	long	fs_maxcontig;		/*   88:max number of contiguous blks */
	long	fs_maxbpg;			/*   92:max number of blks per cyl group */
/* these fields can be computed from the others */
	long	fs_fragshift;		/*   96:block to frag shift */
	long	fs_fsbtodb;			/*  100:fsbtodb and dbtofsb shift constant */
	long	fs_sbsize;			/*  104:actual size of super block */
	long	fs_csmask;			/*  108:csum block offset */
	long	fs_csshift;			/*  112:csum block number */
	long	fs_nindir;			/*  116:value of NINDIR */
	long	fs_inopb;			/*  120:value of INOPB */
	long	fs_nspf;			/*  124:value of NSPF */
/* yet another configuration parameter */
	long	fs_optim;			/*  128:optimization preference, see below */
/* these fields are derived from the hardware */
	long	fs_npsect;			/*  132:# sectors/track including spares */
	long	fs_interleave;		/*  136:hardware sector interleave */
	long	fs_trackskew;		/*  140:sector 0 skew, per track */
	long	fs_headswitch;		/*  144:head switch time, usec */
	long	fs_trkseek;			/*  148:track-to-track seek, usec */
/* sizes determined by number of cylinder groups and their sizes */
	daddr_t fs_csaddr;			/*  152:blk addr of cyl grp summary area */
	long	fs_cssize;			/*  156:size of cyl grp summary area */
	long	fs_cgsize;			/*  160:cylinder group size */
/* these fields are derived from the hardware */
	long	fs_ntrak;			/*  164:tracks per cylinder */
	long	fs_nsect;			/*  168:sectors per track */
	long  	fs_spc;   			/*  172:sectors per cylinder */
/* this comes from the disk driver partitioning */
	long	fs_ncyl;   			/*  176:cylinders in file system */
/* these fields can be computed from the others */
	long	fs_cpg;				/*  180:cylinders per group */
	long	fs_ipg;				/*  184:inodes per group */
	long	fs_fpg;				/*  188:blocks per group * fs_frag */
/* this data must be re-computed after crashes */
	struct	csum fs_old_cstotal;	/*  192:cylinder summary information */
/* these fields are cleared at mount time */
	char   	fs_fmod;    		/*  208:super block modified flag */
	char   	fs_clean;    		/*  209:file system is clean flag */
	char   	fs_ronly;   		/*  210:mounted read-only flag */
	char   	fs_flags;   		/*  211:currently unused flag */
	char	fs_fsmnt[MAXMNTLEN];	/*  212:name mounted on */
/* these fields retain the current block allocation info */
	long	fs_cgrotor;			/*  724:last cg searched */
#if 0
	struct	csum *fs_csp[MAXCSBUFS];/*  728:list of fs_cs info buffers */
#else
	void 	*fs_ocsp[NOCSPTRS];	/*  728:padding; was list of fs_cs buffers */
//	u_char	*fs_contigdirs;		/* # of contiguously allocated dirs */
	struct csum *fs_csp;		/*  848:cg summary info buffer for fs_cs */
	int		*fs_maxcluster;		/*  852:max cluster in each cyl group */
#endif
	long	fs_cpc;				/*  856:cyl per cycle in postbl */
#if 0
	short	fs_opostbl[16][8];	/*  860:old rotation block list head */
	long	fs_sparecon[50];	/* reserved for future constants */
#else
	int		fs_maxbsize;		/*  860:maximum blocking factor permitted */
	quad	fs_sparecon64[17];	/*  864:old rotation block list head */
	quad	fs_sblockloc;		/* 1000:byte offset of standard superblock */
	struct	csum_total fs_cstotal;	/* 1008:cylinder summary information */
	ufs_time_t	fs_time;		/* last time written */
	quad	__fs_size;			/* number of blocks in fs */
	quad	__fs_dsize;			/* number of data blocks in fs */
	ufs2_daddr_t __fs_csaddr;		/* blk addr of cyl grp summary area */
	quad	fs_pendingblocks;	/* blocks in process of being freed */
	int		fs_pendinginodes;	/* inodes in process of being freed */
	int		fs_snapinum[FSMAXSNAP];/* list of snapshot inode numbers */
	int		fs_avgfilesize;		/* expected average file size */
	int		fs_avgfpdir;		/* expected # of files per directory */
	int		fs_save_cgsize;		/* save real cg size to use fs_bsize */
	int		fs_sparecon32[26];	/* reserved for future constants */
	int		__fs_flags;			/* see FS_ flags below */
#endif
	long	fs_contigsumsize;	/* size of cluster summary array */ 
	long	fs_maxsymlinklen;	/* max length of an internal symlink */
	long	fs_inodefmt;		/* format of on-disk inodes */
	u_quad 	fs_maxfilesize;		/* maximum representable file size */
#if 0
	long	fs_state;			/* validate fs_clean field */
	quad	fs_qbmask;			/* ~fs_bmask - for use with quad size */
	quad	fs_qfmask;			/* ~fs_fmask - for use with quad size */
#else
	quad	fs_qbmask;			/* ~fs_bmask - for use with quad size */
	quad	fs_qfmask;			/* ~fs_fmask - for use with quad size */
	long	fs_state;			/* validate fs_clean field */
#endif
	long	fs_postblformat;	/* format of positional layout tables */
	long	fs_nrpos;			/* number of rotaional positions */
	long	fs_postbloff;		/* (short) rotation block list head */
	long	fs_rotbloff;		/* (u_char) blocks for each rotation */
	long	fs_magic;			/* magic number */
	u_char	fs_space[1];		/* list of blocks for each rotation */
/* actually longer */
};

/*
 * Preference for optimization.
 */
#define FS_OPTTIME	0	/* minimize allocation time */
#define FS_OPTSPACE	1	/* minimize disk fragmentation */

/*
 * Rotational layout table format types
 */
#define FS_42POSTBLFMT		-1	/* 4.2BSD rotational table format */
#define FS_DYNAMICPOSTBLFMT	1	/* dynamic rotational table format */
/*
 * Macros for access to superblock array structures
 */
#define fs_postbl(fs, cylno) \
    (((fs)->fs_postblformat == FS_42POSTBLFMT) \
    ? ((fs)->fs_opostbl[cylno]) \
    : ((short *)((char *)(fs) + (fs)->fs_postbloff) + (cylno) * (fs)->fs_nrpos))
#define fs_rotbl(fs) \
    (((fs)->fs_postblformat == FS_42POSTBLFMT) \
    ? ((fs)->fs_space) \
    : ((u_char *)((char *)(fs) + (fs)->fs_rotbloff)))

/*
 * Convert cylinder group to base address of its global summary info.
 *
 * N.B. This macro assumes that sizeof(struct csum) is a power of two.
 */
#if 0
#define fs_cs(fs, indx) \
	fs_csp[(indx) >> (fs)->fs_csshift][(indx) & ~(fs)->fs_csmask]
#else
#define fs_cs(fs, indx) fs_csp[indx]
#endif

/*
 * Cylinder group block for a file system.
 */
#define	CG_MAGIC	0x090255
struct	cg {
	struct	cg *cg_link;		/* linked list of cyl groups */
	long	cg_magic;		/* magic number */
	time_t	cg_time;		/* time last written */
	long	cg_cgx;			/* we are the cgx'th cylinder group */
	short	cg_ncyl;		/* number of cyl's this cg */
	short	cg_niblk;		/* number of inode blocks this cg */
	long	cg_ndblk;		/* number of data blocks this cg */
	struct	csum cg_cs;		/* cylinder summary information */
	long	cg_rotor;		/* position of last used block */
	long	cg_frotor;		/* position of last used frag */
	long	cg_irotor;		/* position of last used inode */
	long	cg_frsum[MAXFRAG];	/* counts of available frags */
	long	cg_btotoff;		/* (long) block totals per cylinder */
	long	cg_boff;		/* (short) free block positions */
	long	cg_iusedoff;		/* (char) used inode map */
	long	cg_freeoff;		/* (u_char) free block map */
	long	cg_nextfreeoff;		/* (u_char) next available space */
	long	cg_sparecon[16];	/* reserved for future use */
	u_char	cg_space[1];		/* space for cylinder group maps */
/* actually longer */
};

/*
 * Macros for access to cylinder group array structures
 */
#define cg_blktot(cgp) \
    (((cgp)->cg_magic != CG_MAGIC) \
    ? (((struct ocg *)(cgp))->cg_btot) \
    : ((long *)((char *)(cgp) + (cgp)->cg_btotoff)))
#define cg_blks(fs, cgp, cylno) \
    (((cgp)->cg_magic != CG_MAGIC) \
    ? (((struct ocg *)(cgp))->cg_b[cylno]) \
    : ((short *)((char *)(cgp) + (cgp)->cg_boff) + (cylno) * (fs)->fs_nrpos))
#define cg_inosused(cgp) \
    (((cgp)->cg_magic != CG_MAGIC) \
    ? (((struct ocg *)(cgp))->cg_iused) \
    : ((char *)((char *)(cgp) + (cgp)->cg_iusedoff)))
#define cg_blksfree(cgp) \
    (((cgp)->cg_magic != CG_MAGIC) \
    ? (((struct ocg *)(cgp))->cg_free) \
    : ((u_char *)((char *)(cgp) + (cgp)->cg_freeoff)))
#define cg_chkmagic(cgp) \
    ((cgp)->cg_magic == CG_MAGIC || ((struct ocg *)(cgp))->cg_magic == CG_MAGIC)

/*
 * The following structure is defined
 * for compatibility with old file systems.
 */
struct	ocg {
	struct	ocg *cg_link;		/* linked list of cyl groups */
	struct	ocg *cg_rlink;		/*     used for incore cyl groups */
	time_t	cg_time;		/* time last written */
	long	cg_cgx;			/* we are the cgx'th cylinder group */
	short	cg_ncyl;		/* number of cyl's this cg */
	short	cg_niblk;		/* number of inode blocks this cg */
	long	cg_ndblk;		/* number of data blocks this cg */
	struct	csum cg_cs;		/* cylinder summary information */
	long	cg_rotor;		/* position of last used block */
	long	cg_frotor;		/* position of last used frag */
	long	cg_irotor;		/* position of last used inode */
	long	cg_frsum[8];		/* counts of available frags */
	long	cg_btot[32];		/* block totals per cylinder */
	short	cg_b[32][8];		/* positions of free blocks */
	char	cg_iused[256];		/* used inode map */
	long	cg_magic;		/* magic number */
	u_char	cg_free[1];		/* free block map */
/* actually longer */
};

/*
 * Turn file system block numbers into disk block addresses.
 * This maps file system blocks to device size blocks.
 */
#define fsbtodb(fs, b)	((b) << (fs)->fs_fsbtodb)
#define	dbtofsb(fs, b)	((b) >> (fs)->fs_fsbtodb)

/*
 * Cylinder group macros to locate things in cylinder groups.
 * They calc file system addresses of cylinder group data structures.
 */
#define	cgbase(fs, c)	((daddr_t)((fs)->fs_fpg * (c)))
#define cgstart(fs, c) \
	(cgbase(fs, c) + (fs)->fs_cgoffset * ((c) & ~((fs)->fs_cgmask)))
#define	cgsblock(fs, c)	(cgstart(fs, c) + (fs)->fs_sblkno)	/* super blk */
#define	cgtod(fs, c)	(cgstart(fs, c) + (fs)->fs_cblkno)	/* cg block */
#define	cgimin(fs, c)	(cgstart(fs, c) + (fs)->fs_iblkno)	/* inode blk */
#define	cgdmin(fs, c)	(cgstart(fs, c) + (fs)->fs_dblkno)	/* 1st data */

/*
 * Macros for handling inode numbers:
 *     inode number to file system block offset.
 *     inode number to cylinder group number.
 *     inode number to file system block address.
 */
#define	itoo(fs, x)	((x) % INOPB(fs))
#define	itog(fs, x)	((x) / (fs)->fs_ipg)
#define	itod(fs, x) \
	((daddr_t)(cgimin(fs, itog(fs, x)) + \
	(blkstofrags((fs), (((x) % (fs)->fs_ipg) / INOPB(fs))))))

/*
 * Give cylinder group number for a file system block.
 * Give cylinder group block number for a file system block.
 */
#define	dtog(fs, d)	((d) / (fs)->fs_fpg)
#define	dtogd(fs, d)	((d) % (fs)->fs_fpg)

/* Bit map related macros. */
#define	setbit(a,i)	((a)[(i)/NBBY] |= 1<<((i)%NBBY))
#define	clrbit(a,i)	((a)[(i)/NBBY] &= ~(1<<((i)%NBBY)))
#define	isset(a,i)	((a)[(i)/NBBY] & (1<<((i)%NBBY)))
#define	isclr(a,i)	(((a)[(i)/NBBY] & (1<<((i)%NBBY))) == 0)

/*
 * Extract the bits for a block from a map.
 * Compute the cylinder and rotational position of a cyl block addr.
 */
#define blkmap(fs, map, loc) \
    (((map)[(loc) / NBBY] >> ((loc) % NBBY)) & (0xff >> (NBBY - (fs)->fs_frag)))
#define cbtocylno(fs, bno) \
    ((bno) * NSPF(fs) / (fs)->fs_spc)
#define cbtorpos(fs, bno) \
    (((bno) * NSPF(fs) % (fs)->fs_spc / (fs)->fs_nsect * (fs)->fs_trackskew + \
     (bno) * NSPF(fs) % (fs)->fs_spc % (fs)->fs_nsect * (fs)->fs_interleave) % \
     (fs)->fs_nsect * (fs)->fs_nrpos / (fs)->fs_npsect)

/*
 * The following macros optimize certain frequently calculated
 * quantities by using shifts and masks in place of divisions
 * modulos and multiplications.
 */
#define blkoff(fs, loc)		/* calculates (loc % fs->fs_bsize) */ \
	((loc) & ~(fs)->fs_bmask)
#define fragoff(fs, loc)	/* calculates (loc % fs->fs_fsize) */ \
	((loc) & ~(fs)->fs_fmask)
#define lblktosize(fs, blk)	/* calculates (blk * fs->fs_bsize) */ \
	((blk) << (fs)->fs_bshift)
#define lblkno(fs, loc)		/* calculates (loc / fs->fs_bsize) */ \
	((loc) >> (fs)->fs_bshift)
#define numfrags(fs, loc)	/* calculates (loc / fs->fs_fsize) */ \
	((loc) >> (fs)->fs_fshift)
#define blkroundup(fs, size)	/* calculates roundup(size, fs->fs_bsize) */ \
	(((size) + (fs)->fs_bsize - 1) & (fs)->fs_bmask)
#define fragroundup(fs, size)	/* calculates roundup(size, fs->fs_fsize) */ \
	(((size) + (fs)->fs_fsize - 1) & (fs)->fs_fmask)
#define fragstoblks(fs, frags)	/* calculates (frags / fs->fs_frag) */ \
	((frags) >> (fs)->fs_fragshift)
#define blkstofrags(fs, blks)	/* calculates (blks * fs->fs_frag) */ \
	((blks) << (fs)->fs_fragshift)
#define fragnum(fs, fsb)	/* calculates (fsb % fs->fs_frag) */ \
	((fsb) & ((fs)->fs_frag - 1))
#define blknum(fs, fsb)		/* calculates rounddown(fsb, fs->fs_frag) */ \
	((fsb) &~ ((fs)->fs_frag - 1))

/*
 * Determine the number of available frags given a
 * percentage to hold in reserve
 */
#define freespace(fs, percentreserved) \
	(blkstofrags((fs), (fs)->fs_cstotal.cs_nbfree) + \
	(fs)->fs_cstotal.cs_nffree - ((fs)->fs_dsize * (percentreserved) / 100))

/*
 * Determining the size of a file block in the file system.
 */
#define blksize(fs, ip, lbn) \
	(((lbn) >= NDADDR || (ip)->i_size >= ((lbn) + 1) << (fs)->fs_bshift) \
	    ? (fs)->fs_bsize \
	    : (fragroundup(fs, blkoff(fs, (ip)->i_size))))
#define dblksize(fs, dip, lbn) \
	(((lbn) >= NDADDR || (dip)->di_size >= ((lbn) + 1) << (fs)->fs_bshift) \
	    ? (fs)->fs_bsize \
	    : (fragroundup(fs, blkoff(fs, (dip)->di_size))))

/*
 * Number of disk sectors per block; assumes DEV_BSIZE byte sector size.
 */
#define	NSPB(fs)	((fs)->fs_nspf << (fs)->fs_fragshift)
#define	NSPF(fs)	((fs)->fs_nspf)

/*
 * INOPB is the number of inodes in a secondary storage block.
 */
#define	INOPB(fs)	((fs)->fs_inopb)
#define	INOPF(fs)	((fs)->fs_inopb >> (fs)->fs_fragshift)

/*
 * NINDIR is the number of indirects in a file system block.
 */
#define	NINDIR(fs)	((fs)->fs_nindir)
