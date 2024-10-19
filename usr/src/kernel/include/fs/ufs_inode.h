/*
 * Copyright (c) 1982, 1989 The Regents of the University of California.
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
 *	@(#)inode.h	7.17 (Berkeley) 5/8/91
 */

#include "ufs_dinode.h"

/*
 * The inode is used to describe each active (or recently active)
 * file in the UFS filesystem. It is composed of two types of
 * information. The first part is the information that is needed
 * only while the file is active (such as the identity of the file
 * and linkage to speed its lookup). The second part is the 
 * permannent meta-data associated with the file which is read
 * in from the permanent dinode from long term storage when the
 * file becomes active, and is put back when the file is no longer
 * being used.
 */
struct inode {
	struct	inode *i_chain[2]; /* hash chain, MUST be first */
	struct	vnode *i_vnode;	/* vnode associated with this inode */
	struct	vnode *i_devvp;	/* vnode for block I/O */
	u_long	i_flag;		/* see below */
	dev_t	i_dev;		/* device where inode resides */
	ino_t	i_number;	/* the identity of the inode */
	struct	fs *i_fs;	/* filesystem associated with this inode */
	struct	dquot *i_dquot[MAXQUOTAS]; /* pointer to dquot structures */
	struct	lockf *i_lockf;	/* head of byte-level lock list */
	long	i_diroff;	/* offset in dir, where we found last entry */
	off_t	i_endoff;	/* end of useful stuff in directory */
	long	i_spare0;
	long	i_spare1;
	struct	dinode i_din;	/* the on-disk dinode */
};

/*---------------------------------------------------------------------------*/
#if 0
#define	TAILQ_ENTRY(type)						\
struct {								\
	struct type *tqe_next;	/* next element */			\
	struct type **tqe_prev;	/* address of previous next element */	\
}

struct vn_clusterw {
	daddr_t	v_cstart;			/* v start block of cluster */
	daddr_t	v_lasta;			/* v last allocation  */
	daddr_t	v_lastw;			/* v last write  */
	int	v_clen;				/* v length of cur. cluster */
};

struct inode {
	struct genfs_node i_gnode;
	TAILQ_ENTRY(inode) i_nextsnap; /* snapshot file list. */
	struct vnode 	*i_vnode;	/* Vnode associated with this inode. */
	struct ufsmount *i_ump; /* Mount point associated with this inode. */
	struct vnode 	*i_devvp;	/* Vnode for block I/O. */
	u_long			i_flag;	/* flags, see below */
	dev_t			i_dev;	/* Device associated with the inode. */
	ino_t			i_number;	/* The identity of the inode. */

	union {			/* Associated filesystem. */
		struct	fs *fs;		/* FFS */
		struct	lfs *lfs;	/* LFS */
		struct	m_ext2fs *e2fs;	/* EXT2FS */
	} inode_u;
#define	i_fs	inode_u.fs
#define	i_lfs	inode_u.lfs
#define	i_e2fs	inode_u.e2fs

	void			*i_unused1;	/* Unused. */
	struct dquot	*i_dquot[MAXQUOTAS]; /* Dquot structures. */
	u_quad			i_modrev;	/* Revision level for NFS lease. */
	struct lockf	*i_lockf;/* Head of byte-level lock list. */

	/*
	 * Side effects; used during (and after) directory lookup.
	 * XXX should not be here.
	 */
	struct ufs_lookup_results i_crap;
	unsigned i_crapcounter;	/* serial number for i_crap */

	/*
	 * Inode extensions
	 */
	union {
		/* Other extensions could go here... */
		struct	ffs_inode_ext ffs;
		struct	ext2fs_inode_ext e2fs;
		struct  lfs_inode_ext *lfs;
	} inode_ext;
#define	i_snapblklist		inode_ext.ffs.ffs_snapblklist
#define	i_ffs_first_data_blk	inode_ext.ffs.ffs_first_data_blk
#define	i_ffs_first_indir_blk	inode_ext.ffs.ffs_first_indir_blk
#define	i_e2fs_last_lblk	inode_ext.e2fs.ext2fs_last_lblk
#define	i_e2fs_last_blk		inode_ext.e2fs.ext2fs_last_blk
	/*
	 * Copies from the on-disk dinode itself.
	 *
	 * These fields are currently only used by FFS and LFS,
	 * do NOT use them with ext2fs.
	 */
	u_short		i_mode;		/* IFMT, permissions; see dinode.h. */
	short		i_nlink;	/* File link count. */
	u_quad		i_size;		/* File byte count. */
	u_long		i_flags;	/* Status flags (chflags). */
	long		i_gen;		/* Generation number. */
	u_long		i_uid;		/* File owner. */
	u_long		i_gid;		/* File group. */
	u_short		i_omode;	/* Old mode, for ufs_reclaim. */

	struct dirhash *i_dirhash;	/* Hashing for large directories */

	/*
	 * Data for extended attribute modification.
 	 */
	u_char	  *i_ea_area;	/* Pointer to malloced copy of EA area */
	unsigned  i_ea_len;	/* Length of i_ea_area */
	int	  i_ea_error;	/* First errno in transaction */
	int	  i_ea_refs;	/* Number of users of EA area */

	/*
	 * The on-disk dinode itself.
	 */
	union {
		struct	ufs1_dinode *ffs1_din;	/* 128 bytes of the on-disk dinode. */
		struct	ufs2_dinode *ffs2_din;
		struct	ext2fs_dinode *e2fs_din; /* 128 bytes of the on-disk
						   dinode. */
	} i_din;
};
#endif
/*---------------------------------------------------------------------------*/

#define	i_mode		i_din.di_mode
#define	i_nlink		i_din.di_nlink
#define	i_uid		i_din.di_uid
#define	i_gid		i_din.di_gid
#if BYTE_ORDER == LITTLE_ENDIAN || defined(tahoe) /* ugh! -- must be fixed */
//#define	i_size		i_din.di_qsize.val[0]
#define	i_size		i_din.di_qsize
#else /* BYTE_ORDER == BIG_ENDIAN */
#define	i_size		i_din.di_qsize.val[1]
#endif
#define	i_db		i_din.di_db
#define	i_ib		i_din.di_ib
#define	i_atime		i_din.di_atime
#define	i_mtime		i_din.di_mtime
#define	i_ctime		i_din.di_ctime
#define i_blocks	i_din.di_blocks
#define	i_rdev		i_din.di_db[0]
#define i_flags		i_din.di_flags
#define i_gen		i_din.di_gen
#define	i_forw		i_chain[0]
#define	i_back		i_chain[1]

/* flags */
#define	ILOCKED		0x0001		/* inode is locked */
#define	IWANT		0x0002		/* some process waiting on lock */
#define	IRENAME		0x0004		/* inode is being renamed */
#define	IUPD		0x0010		/* file has been modified */
#define	IACC		0x0020		/* inode access time to be updated */
#define	ICHG		0x0040		/* inode has been changed */
#define	IMOD		0x0080		/* inode has been modified */
#define	ISHLOCK		0x0100		/* file has shared lock */
#define	IEXLOCK		0x0200		/* file has exclusive lock */
#define	ILWAIT		0x0400		/* someone waiting on file lock */

#ifdef KERNEL
/*
 * Convert between inode pointers and vnode pointers
 */
#define VTOI(vp)	((struct inode *)(vp)->v_data)
#define ITOV(ip)	((ip)->i_vnode)

/*
 * Convert between vnode types and inode formats
 */
#define IFTOVT(mode)	(iftovt_tab[((mode) & IFMT) >> 12])
#define VTTOIF(indx)	(vttoif_tab[(int)(indx)])
#define MAKEIMODE(indx, mode)	(int)(VTTOIF(indx) | (mode))

#ifdef INODE_TYPE_TABLE
enum vtype iftovt_tab[16] = {
	VNON, VFIFO, VCHR, VNON, VDIR, VNON, VBLK, VNON,
	VREG, VNON, VLNK, VNON, VSOCK, VNON, VNON, VBAD,
};
int	vttoif_tab[9] = {
	0, IFREG, IFDIR, IFBLK, IFCHR, IFLNK, IFSOCK, IFIFO, IFMT,
};
#else
extern enum vtype	iftovt_tab[];
extern int		vttoif_tab[];
#endif

extern u_long	nextgennumber;		/* next generation number to assign */

/* extern ino_t	dirpref(); */
extern ino_t dirpref(register struct fs* fs);

/*
 * Lock and unlock inodes.
 */
#if 0
#ifdef notdef
#define	ILOCK(ip) { \
	while ((ip)->i_flag & ILOCKED) { \
		(ip)->i_flag |= IWANT; \
		(void) tsleep((caddr_t)(ip), PINOD, "ilock", 0); \
	} \
	(ip)->i_flag |= ILOCKED; \
}

#define	IUNLOCK(ip) { \
	(ip)->i_flag &= ~ILOCKED; \
	if ((ip)->i_flag&IWANT) { \
		(ip)->i_flag &= ~IWANT; \
		wakeup((caddr_t)(ip)); \
	} \
}
#else
#define ILOCK(ip)	ilock(ip)
#define IUNLOCK(ip)	iunlock(ip)
#endif
#endif

#define	IUPDAT(ip, t1, t2, waitfor) { \
	if (ip->i_flag&(IUPD|IACC|ICHG|IMOD)) \
		(void) iupdat(ip, t1, t2, waitfor); \
}

#define	ITIMES(ip, t1, t2) { \
	if ((ip)->i_flag&(IUPD|IACC|ICHG)) { \
		(ip)->i_flag |= IMOD; \
		if ((ip)->i_flag&IACC) \
			(ip)->i_atime = (t1)->tv_sec; \
		if ((ip)->i_flag&IUPD) \
			(ip)->i_mtime = (t2)->tv_sec; \
		if ((ip)->i_flag&ICHG) \
			(ip)->i_ctime = time.tv_sec; \
		(ip)->i_flag &= ~(IACC|IUPD|ICHG); \
	} \
}

/*
 * This overlays the fid sturcture (see mount.h)
 */
struct ufid {
	u_short	ufid_len;	/* length of structure */
	u_short	ufid_pad;	/* force long alignment */
	ino_t	ufid_ino;	/* file number (ino) */
	long	ufid_gen;	/* generation number */
};

/*
 * Prototypes for UFS vnode operations
 */
int ufs_lookup __P((struct vnode *vp, struct nameidata *ndp, struct proc *p));
int ufs_create __P((struct nameidata *ndp, struct vattr *vap, struct proc *p));
int ufs_mknod __P((struct nameidata *ndp, struct vattr *vap, struct ucred *cred,
	struct proc *p));
int ufs_open __P((struct vnode *vp, int mode, struct ucred *cred,
	struct proc *p));
int ufs_close __P((struct vnode *vp, int fflag, struct ucred *cred,
	struct proc *p));
int ufs_access __P((struct vnode *vp, int mode, struct ucred *cred,
	struct proc *p));
int ufs_getattr __P((struct vnode *vp, struct vattr *vap, struct ucred *cred,
	struct proc *p));
int ufs_setattr __P((struct vnode *vp, struct vattr *vap, struct ucred *cred,
	struct proc *p));
int ufs_read __P((struct vnode *vp, struct uio *uio, int ioflag,
	struct ucred *cred));
int ufs_write __P((struct vnode *vp, struct uio *uio, int ioflag,
	struct ucred *cred));
int ufs_ioctl __P((struct vnode *vp, int command, caddr_t data, int fflag,
	struct ucred *cred, struct proc *p));
int ufs_select __P((struct vnode *vp, int which, int fflags, struct ucred *cred,
	struct proc *p));
int ufs_mmap __P((struct vnode *vp, int fflags, struct ucred *cred,
	struct proc *p));
int ufs_fsync __P((struct vnode *vp, int fflags, struct ucred *cred,
	int waitfor, struct proc *p));
int ufs_seek __P((struct vnode *vp, off_t oldoff, off_t newoff,
	struct ucred *cred));
int ufs_remove __P((struct nameidata *ndp, struct proc *p));
int ufs_link __P((struct vnode *vp, struct nameidata *ndp, struct proc *p));
int ufs_rename __P((struct nameidata *fndp, struct nameidata *tdnp,
	struct proc *p));
int ufs_mkdir __P((struct nameidata *ndp, struct vattr *vap, struct proc *p));
int ufs_rmdir __P((struct nameidata *ndp, struct proc *p));
int ufs_symlink __P((struct nameidata *ndp, struct vattr *vap, char *target,
	struct proc *p));
int ufs_readdir __P((struct vnode *vp, struct uio *uio, struct ucred *cred,
	int *eofflagp));
int ufs_readlink __P((struct vnode *vp, struct uio *uio, struct ucred *cred));
int ufs_abortop __P((struct nameidata *ndp));
int ufs_inactive __P((struct vnode *vp, struct proc *p));
int ufs_reclaim __P((struct vnode *vp));
int ufs_lock __P((struct vnode *vp));
int ufs_unlock __P((struct vnode *vp));
int ufs_bmap __P((struct vnode *vp, daddr_t bn, struct vnode **vpp,
	daddr_t *bnp));
int ufs_strategy __P((struct buf *bp));
int ufs_print __P((struct vnode *vp));
int ufs_islocked __P((struct vnode *vp));
int ufs_advlock __P((struct vnode *vp, caddr_t id, int op, struct flock *fl,
	int flags));
#endif /* KERNEL */
