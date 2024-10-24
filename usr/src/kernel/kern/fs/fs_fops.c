/*
 * Copyright (c) 1982, 1986, 1989 Regents of the University of California.
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
 * $Id: fs_fops.c,v 1.1 94/10/19 17:09:18 bill Exp Locker: bill $
 */

#include "sys/param.h"
#include "sys/file.h"
#include "sys/stat.h"
#include "sys/mount.h"
#include "sys/ioctl.h"
#include "sys/errno.h"
#include "buf.h"
#include "proc.h"
#include "resourcevar.h"
#include "tty.h"
#include "uio.h"
#include "signalvar.h"

#include "namei.h"
#include "vnode.h"

#include "prototypes.h"

int vn_writechk(struct vnode *vp);

static int
	vn_read(struct file *fp, struct uio *uio, struct ucred *cred),
	vn_write(struct file *fp, struct uio *uio, struct ucred *cred),
	vn_ioctl(struct file *fp, int com, caddr_t data, struct proc *p),
	vn_select(struct file *fp, int which, struct proc *p),
	vn_closefile(struct file *fp, struct proc *p),
	vn_statfile(struct file *fp, struct stat *s, struct proc *p);
extern int vn_stat(struct vnode*, struct stat*, struct proc*);

struct 	fileops vnops =
	{ vn_read, vn_write, vn_ioctl, vn_select, vn_closefile, vn_statfile };

/*
 * Common code for vnode open operations.
 * Check permissions, and call the VOP_OPEN or VOP_CREATE routine.
 */
int
vn_open(struct nameidata *ndp, struct proc *p, int fmode, int cmode)
{
	struct vnode *vp;
	struct ucred *cred = p->p_ucred;
	struct vattr vat, *vap = &vat;
	int error;

	if (fmode & O_CREAT) {
		ndp->ni_nameiop = CREATE | LOCKPARENT | LOCKLEAF;
		if ((fmode & O_EXCL) == 0)
			ndp->ni_nameiop |= FOLLOW;
		error = namei(ndp, p);
		if (error != 0)
			return (error);
		if (ndp->ni_vp == NULL) {
			VATTR_NULL(vap);
			vap->va_type = VREG;
			vap->va_mode = cmode;
			error = VOP_CREATE(ndp, vap, p);
			if (error != 0)
				return (error);
			fmode &= ~O_TRUNC;
			vp = ndp->ni_vp;
		} else {
			VOP_ABORTOP(ndp);
			if (ndp->ni_dvp == ndp->ni_vp)
				vrele(ndp->ni_dvp);
			else
				vput(ndp->ni_dvp);
			ndp->ni_dvp = NULL;
			vp = ndp->ni_vp;
			if (fmode & O_EXCL) {
				error = EEXIST;
				goto bad;
			}
			fmode &= ~O_CREAT;
		}
	} else {
		ndp->ni_nameiop = LOOKUP | FOLLOW | LOCKLEAF;
		error = namei(ndp, p);
		if (error != 0)
			return (error);
		vp = ndp->ni_vp;
	}
	if (vp->v_type == VSOCK) {
		error = EOPNOTSUPP;
		goto bad;
	}
	if ((fmode & O_CREAT) == 0) {
		if (fmode & FREAD) {
			error = VOP_ACCESS(vp, VREAD, cred, p);
			if (error != 0)
				goto bad;
		}
		if (fmode & (FWRITE | O_TRUNC)) {
			if (vp->v_type == VDIR) {
				error = EISDIR;
				goto bad;
			}
			if ((error = vn_writechk(vp)) ||
			    (error = VOP_ACCESS(vp, VWRITE, cred, p)))
				goto bad;
		}
	}
	if (fmode & O_TRUNC) {
		VATTR_NULL(vap);
		vap->va_size = 0;
		error = VOP_SETATTR(vp, vap, cred, p);
		if (error != 0)
			goto bad;
	}
	error = VOP_OPEN(vp, fmode, cred, p);
	if (error != 0)
		goto bad;
	if (fmode & FWRITE)
		vp->v_writecount++;
	return (0);
bad:
	vput(vp);
	return (error);
}

/*
 * Check for write permissions on the specified vnode.
 * The read-only status of the file system is checked.
 * Also, prototype text segments cannot be written.
 */
int
vn_writechk(struct vnode *vp)
{

	/*
	 * Disallow write attempts on read-only file systems;
	 * unless the file is a socket or a block or character
	 * device resident on the file system.
	 */
	if (vp->v_mount->mnt_flag & MNT_RDONLY) {
		switch ((int)vp->v_type) {
		case VREG: case VDIR: case VLNK:
			return (EROFS);
		}
	}
	/*
	 * If there's shared text associated with
	 * the vnode, try to free it up once.  If
	 * we fail, we can't allow writing.
	 */
	if ((vp->v_flag & VTEXT) && !vnode_pager_uncache(vp))
		return (ETXTBSY);
	return (0);
}

/*
 * Vnode close call
 */
int vn_close(register struct vnode* vp, int flags, struct ucred* cred, struct proc* p)
{
	int error;

	if (flags & FWRITE)
		vp->v_writecount--;
	error = VOP_CLOSE(vp, flags, cred, p);
	vrele(vp);
	return (error);
}

/*
 * Package up an I/O request on a vnode into a uio and do it.
 * [internal interface to file i/o for kernel only]
 */
int vn_rdwr(enum uio_rw rw, struct vnode* vp, caddr_t base, int len, off_t offset, enum uio_seg segflg,
			int ioflg, struct ucred* cred, int* aresid, struct proc* p)
{
	struct uio auio;
	struct iovec aiov;
	int error;

	if ((ioflg & IO_NODELOCKED) == 0)
		VOP_LOCK(vp);
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	aiov.iov_base = base;
	aiov.iov_len = len;
	auio.uio_resid = len;
	auio.uio_offset = offset;
	auio.uio_segflg = segflg;
	auio.uio_rw = rw;
	auio.uio_procp = p;
	if (rw == UIO_READ)
		error = VOP_READ(vp, &auio, ioflg, cred);
	else
		error = VOP_WRITE(vp, &auio, ioflg, cred);
	if (aresid)
		*aresid = auio.uio_resid;
	else
		if (auio.uio_resid && error == 0)
			error = EIO;
	if ((ioflg & IO_NODELOCKED) == 0)
		VOP_UNLOCK(vp);
	return (error);
}

/*
 * File table vnode read routine.
 */
static int
vn_read(struct file *fp, struct uio *uio, struct ucred *cred)
{
	struct vnode *vp = (struct vnode *)fp->f_data;
	int count, error;

	VOP_LOCK(vp);
	uio->uio_offset = fp->f_offset;
	count = uio->uio_resid;
	error = VOP_READ(vp, uio, (fp->f_flag & FNONBLOCK) ? IO_NDELAY : 0,
		cred);
	fp->f_offset += count - uio->uio_resid;
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * File table vnode write routine.
 */
static int
vn_write(struct file *fp, struct uio *uio, struct ucred *cred)
{
	struct vnode *vp = (struct vnode *)fp->f_data;
	int count, error, ioflag = 0;

	if (vp->v_type == VREG && (fp->f_flag & O_APPEND))
		ioflag |= IO_APPEND;
	if (fp->f_flag & FNONBLOCK)
		ioflag |= IO_NDELAY;

	/* check for if within file write size limit */
	if (vp->v_type == VREG && 
	    uio->uio_offset + uio->uio_resid >
	      uio->uio_procp->p_rlimit[RLIMIT_FSIZE].rlim_cur) {
		psignal(uio->uio_procp, SIGXFSZ);
		return (EFBIG);
	}

	VOP_LOCK(vp);
	uio->uio_offset = fp->f_offset;
	count = uio->uio_resid;
	error = VOP_WRITE(vp, uio, ioflag, cred);
	if (ioflag & IO_APPEND)
		fp->f_offset = uio->uio_offset;
	else
		fp->f_offset += count - uio->uio_resid;
	VOP_UNLOCK(vp);
	return (error);
}

/*
 * File table vnode stat routine.
 */
static int
vn_statfile(struct file *fp, struct stat *sb, struct proc *p)
{
	return (vn_stat(((struct vnode *)fp->f_data), sb, p));
}

/*
 * Vnode stat routine.
 */
int
vn_stat(struct vnode *vp, struct stat *sb, struct proc *p)
{
	struct vattr vattr, *vap;
	int error;
	u_short mode;

	vap = &vattr;
	error = VOP_GETATTR(vp, vap, p->p_ucred, p);
	if (error)
		return (error);
	/*
	 * Copy from vattr table
	 */
	sb->st_dev = vap->va_fsid;
	sb->st_ino = vap->va_fileid;
	mode = vap->va_mode;
	switch (vp->v_type) {
	case VREG:
		mode |= S_IFREG;
		break;
	case VDIR:
		mode |= S_IFDIR;
		break;
	case VBLK:
		mode |= S_IFBLK;
		break;
	case VCHR:
		mode |= S_IFCHR;
		break;
	case VLNK:
		mode |= S_IFLNK;
		break;
	case VSOCK:
		mode |= S_IFSOCK;
		break;
	case VFIFO:
		mode |= S_IFIFO;
		break;
	default:
		return (EBADF);
	};
	sb->st_mode = mode;
	sb->st_nlink = vap->va_nlink;
	sb->st_uid = vap->va_uid;
	sb->st_gid = vap->va_gid;
	sb->st_rdev = vap->va_rdev;
	sb->st_size = vap->va_size;
	sb->st_atime = vap->va_atime.tv_sec;
	sb->st_atimensec = 0;
	sb->st_mtime = vap->va_mtime.tv_sec;
	sb->st_mtimensec = 0;
	sb->st_ctime = vap->va_ctime.tv_sec;
	sb->st_ctimensec = 0;
	sb->st_blksize = vap->va_blocksize;
	sb->st_flags = vap->va_flags;
	sb->st_gen = vap->va_gen;
	sb->st_blocks = vap->va_bytes / S_BLKSIZE;
	return (0);
}

/*
 * File table vnode ioctl routine.
 */
static int
vn_ioctl(struct file *fp, int com, caddr_t data, struct proc *p)
{
	struct vnode *vp = ((struct vnode *)fp->f_data);
	struct vattr vattr;
	int error;

	switch (vp->v_type) {

	case VREG:
	case VDIR:
		if (com == FIONREAD) {
			error = VOP_GETATTR(vp, &vattr, p->p_ucred, p);
			if (error != 0)
				return (error);
			*(off_t *)data = vattr.va_size - fp->f_offset;
			return (0);
		}
		if (com == FIONBIO || com == FIOASYNC)	/* XXX */
			return (0);			/* XXX */
		/* fall into ... */

	default:
		return (ENOTTY);

	case VFIFO:
	case VCHR:
	case VBLK:
		error = VOP_IOCTL(vp, com, data, fp->f_flag, p->p_ucred, p);
		if (error == 0 && com == TIOCSCTTY) {
			p->p_session->s_ttyvp = vp;
			VREF(vp);
		}
		return (error);
	}
}

/*
 * File table vnode select routine.
 */
static int
vn_select(struct file *fp, int which, struct proc *p)
{

	return (VOP_SELECT(((struct vnode *)fp->f_data), which, fp->f_flag,
		fp->f_cred, p));
}

/*
 * File table vnode close routine.
 */
static int
vn_closefile(struct file *fp, struct proc *p)
{

	return (vn_close(((struct vnode *)fp->f_data), fp->f_flag,
		fp->f_cred, p));
}

/*
 * vn_fhtovp() - convert a fh to a vnode ptr (optionally locked)
 * 	- look up fsid in mount list (if not found ret error)
 *	- get vp by calling VFS_FHTOVP() macro
 *	- if lockflag lock it with VOP_LOCK()
 */
int
vn_fhtovp(fhandle_t *fhp, int lockflag, struct vnode **vpp)
{
	register struct mount *mp;

	if ((mp = getvfs(&fhp->fh_fsid)) == NULL)
		return (ESTALE);
	if (VFS_FHTOVP(mp, &fhp->fh_fid, vpp))
		return (ESTALE);
	if (!lockflag)
		VOP_UNLOCK(*vpp);
	return (0);
}
