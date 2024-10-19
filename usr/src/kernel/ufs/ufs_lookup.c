/*
 * Copyright (c) 1989 The Regents of the University of California.
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
 *	$Id: ufs_lookup.c,v 1.1 94/10/20 10:56:41 root Exp $
 */

#include "sys/param.h"
#include "sys/mount.h"
#include "sys/file.h"
#include "ucred.h"
#include "sys/time.h"
#include "uio.h"
#include "sys/errno.h"
#include "buf.h"

#include "namei.h"
#include "vnode.h"
#include "ufs_quota.h"
#include "ufs_inode.h"
#include "ufs_dir.h"
#include "ufs.h"

#include "prototypes.h"

#define	IFTODT(mode)	(((mode) & 0170000) >> 12)

struct	nchstats nchstats;
#ifdef DIAGNOSTIC
int	dirchk = 1;
#else
int	dirchk = 0;
#endif

extern void dirbad(struct inode* ip, off_t offset, char* how);
extern int iget(struct inode* xp, ino_t ino, struct inode** ipp);
extern int iput(register struct inode* ip);
extern int itrunc(register struct inode*, u_long, int);
extern void ilock(register struct inode*);
extern void iunlock(register struct inode*);

static int blkatoff(struct inode* ip, off_t offset, char** res, struct buf** bpp);
static int dirbadentry(register struct direct*ep, int entryoffsetinblock);

/* true if old FS format...*/
#define OFSFMT(vp)	((vp)->v_mount->mnt_maxsymlinklen <= 0)

/*
 * Convert a component of a pathname into a pointer to a locked inode.
 * This is a very central and rather complicated routine.
 * If the file system is not maintained in a strict tree hierarchy,
 * this can result in a deadlock situation (see comments in code below).
 *
 * The flag argument is LOOKUP, CREATE, RENAME, or DELETE depending on
 * whether the name is to be looked up, created, renamed, or deleted.
 * When CREATE, RENAME, or DELETE is specified, information usable in
 * creating, renaming, or deleting a directory entry may be calculated.
 * If flag has LOCKPARENT or'ed into it and the target of the pathname
 * exists, lookup returns both the target and its parent directory locked.
 * When creating or renaming and LOCKPARENT is specified, the target may
 * not be ".".  When deleting and LOCKPARENT is specified, the target may
 * be "."., but the caller must check to ensure it does an vrele and iput
 * instead of two iputs.
 *
 * Overall outline of ufs_lookup:
 *
 *	check accessibility of directory
 *	look for name in cache, if found, then if at end of path
 *	  and deleting or creating, drop it, else return name
 *	search for name in directory, to found or notfound
 * notfound:
 *	if creating, return locked directory, leaving info on available slots
 *	else return error
 * found:
 *	if at end of path and deleting, return information to allow delete
 *	if at end of path and rewriting (RENAME and LOCKPARENT), lock target
 *	  inode and return info to allow rewrite
 *	if not at end, add name to cache; if at end and neither creating
 *	  nor deleting, add name to cache
 *
 * NOTE: (LOOKUP | LOCKPARENT) currently returns the parent inode unlocked.
パス名のコンポーネントをロックされた inode へのポインタに変換します。
これは非常に中心的で、かなり複雑なルーチンです。ファイル システムが
厳密なツリー階層で維持されていない場合、デッドロック状態になる可能性
があります (以下のコード内のコメントを参照)。

フラグ引数は、名前が検索、作成、名前変更、または削除されるかどうかに
応じて、LOOKUP、CREATE、RENAME、または DELETE になります。CREATE、
RENAME、または DELETE が指定されている場合、ディレクトリ エントリの
作成、名前変更、または削除に使用できる情報が計算される場合があります。
フラグに LOCKPARENT が or で結合されていて、パス名のターゲットが存在
する場合、lookup はターゲットとロックされた親ディレクトリの両方を返
します。作成または名前変更時に LOCKPARENT が指定されている場合、
ターゲットは "." ではない可能性があります。削除時に LOCKPARENT が指定
されている場合、ターゲットは "." である可能性がありますが、呼び出し側
は、2 つの iput ではなく、vrele と iput を実行することを確認する必要
があります。

ufs_lookup の全体的な概要:

ディレクトリのアクセス可能性をチェックします。

キャッシュで名前を検索し、見つかった場合はパスの末尾で削除または作成中
の場合は削除し、そうでない場合は名前を返します。
ディレクトリで名前を検索し、見つかったか見つからないかを確認します。
見つからない場合:
作成中の場合はロックされたディレクトリを返し、使用可能なスロットに関す
る情報を残します。そうでない場合はエラーを返します。
見つかった場合:
パスの末尾で削除中の場合は削除を許可する情報を返します。
パスの末尾で書き換え中の場合は (RENAME および LOCKPARENT)、ターゲットを
ロックします。
inode と書き換えを許可する情報を返します。
末尾でない場合は名前をキャッシュに追加します。末尾で作成も削除もされて
いない場合は名前をキャッシュに追加します。

注意: (LOOKUP | LOCKPARENT) は現在、ロック解除された親inode を返します。
*/
int ufs_lookup(register struct vnode* vdp, register struct nameidata* ndp, struct proc* p)
{
	register struct inode *dp;	/* the directory we are searching */
	register struct fs *fs;		/* file system that directory is in */
	struct buf *bp = 0;			/* a buffer of directory entries */
	register struct direct *ep;	/* the current directory entry */
	int entryoffsetinblock;		/* offset of ep in bp's buffer */
	enum {NONE, COMPACT, FOUND} slotstatus;
	int slotoffset = -1;		/* offset of area with free space */
	int slotsize;				/* size of area at slotoffset */
	int slotfreespace;			/* amount of space free in slot */
	int slotneeded;				/* size of the entry we're seeking */
	int numdirpasses;			/* strategy for directory search */
	int endsearch;				/* offset to end directory search */
	int prevoff;				/* ndp->ni_ufs.ufs_offset of previous entry */
	struct inode *pdp;			/* saved dp during symlink work */
	struct inode *tdp;			/* returned by iget */
	off_t enduseful;			/* pointer past last used dir slot */
	int flag;					/* LOOKUP, CREATE, RENAME, or DELETE */
	int lockparent;				/* 1 => lockparent flag is set */
	int wantparent;				/* 1 => wantparent or lockparent flag */
	int error;

	ndp->ni_dvp = vdp;
	ndp->ni_vp = NULL;
	dp = VTOI(vdp);
	fs = dp->i_fs;
	lockparent = ndp->ni_nameiop & LOCKPARENT;
	flag = ndp->ni_nameiop & OPMASK;
	wantparent = ndp->ni_nameiop & (LOCKPARENT|WANTPARENT);

	/*
	 * Check accessiblity of directory.
	 */
	if ((dp->i_mode&IFMT) != IFDIR)
		return (ENOTDIR);
	error = ufs_access(vdp, VEXEC, ndp->ni_cred, p);
	if (error != 0)
		return (error);

	/*
	 * We now have a segment name to search for, and a directory to search.
	 *
	 * Before tediously performing a linear scan of the directory,
	 * check the name cache to see if the directory/name pair
	 * we are looking for is known already.
	 */
	error = cache_lookup(ndp);
	if (error != 0) {
		int vpid;	/* capability number of vnode */

		if (error == ENOENT)
			return (error);
#ifdef PARANOID
		if (vdp == ndp->ni_rdir && ndp->ni_isdotdot)
			panic("ufs_lookup: .. through root");
#endif
		/*
		 * Get the next vnode in the path.
		 * See comment below starting `Step through' for
		 * an explaination of the locking protocol.
		 */
		pdp = dp;
		dp = VTOI(ndp->ni_vp);
		vdp = ndp->ni_vp;
		vpid = vdp->v_id;
		if (pdp == dp) {
			VREF(vdp);
			error = 0;
		} else if (ndp->ni_isdotdot) {
			iunlock(pdp);
			error = vget(vdp);
			if (!error && lockparent && *ndp->ni_next == '\0')
				ilock(pdp);
		} else {
			error = vget(vdp);
			if (!lockparent || error || *ndp->ni_next != '\0')
				iunlock(pdp);
		}
		/*
		 * Check that the capability number did not change
		 * while we were waiting for the lock.
		 */
		if (!error) {
			if (vpid == vdp->v_id)
				return (0);
			iput(dp);
			if (lockparent && pdp != dp && *ndp->ni_next == '\0')
				iunlock(pdp);
		}
		ilock(pdp);
		dp = pdp;
		vdp = ITOV(dp);
		ndp->ni_vp = NULL;
	}

	/*
	 * If there is cached information on a previous search of
	 * this directory, pick up where we last left off.
	 * We cache only lookups as these are the most common
	 * and have the greatest payoff. Caching CREATE has little
	 * benefit as it usually must search the entire directory
	 * to determine that the entry does not exist. Caching the
	 * location of the last DELETE or RENAME has not reduced
	 * profiling time and hence has been removed in the interest
	 * of simplicity.
	このディレクトリの以前の検索でキャッシュされた情報がある場合は、最後に
	中断したところから再開します。検索のみをキャッシュするのは、これが最も
	一般的で最大の効果があるためです。CREATE をキャッシュしても、エントリ
	が存在しないことを確認するために通常はディレクトリ全体を検索する必要が
	あるため、あまりメリットはありません。最後の DELETE または RENAME の場
	所をキャッシュしてもプロファイリング時間は短縮されないため、簡素化のた
	めに削除されました。
	 */
	slotstatus = FOUND;
	if ((flag == CREATE || flag == RENAME) && *ndp->ni_next == 0) {
		slotstatus = NONE;
		slotfreespace = 0;
		slotneeded = ((sizeof (struct direct) - (MAXNAMLEN + 1)) +
			((ndp->ni_namelen + 1 + 3) &~ 3));
	}

	/*
	 * If there is cached information on a previous search of
	 * this directory, pick up where we last left off.
	 * We cache only lookups as these are the most common
	 * and have the greatest payoff. Caching CREATE has little
	 * benefit as it usually must search the entire directory
	 * to determine that the entry does not exist. Caching the
	 * location of the last DELETE or RENAME has not reduced
	 * profiling time and hence has been removed in the interest
	 * of simplicity.
	このディレクトリの以前の検索でキャッシュされた情報がある場合は、最後に
	中断したところから再開します。検索のみをキャッシュするのは、これが最も
	一般的で最大の効果があるためです。CREATE をキャッシュしても、エントリ
	が存在しないことを確認するために通常はディレクトリ全体を検索する必要が
	あるため、あまりメリットはありません。最後の DELETE または RENAME の場
	所をキャッシュしてもプロファイリング時間は短縮されないため、簡素化のた
	めに削除されました。
	 */
	if (flag != LOOKUP || dp->i_diroff == 0 || dp->i_diroff > dp->i_size) {
		ndp->ni_ufs.ufs_offset = 0;
		numdirpasses = 1;
	} else {
		ndp->ni_ufs.ufs_offset = dp->i_diroff;
		entryoffsetinblock = blkoff(fs, ndp->ni_ufs.ufs_offset);
		if (entryoffsetinblock != 0) {
			error = blkatoff(dp, ndp->ni_ufs.ufs_offset, (char **)0, &bp);
			if (error != 0)
				return (error);
		}
		numdirpasses = 2;
		nchstats.ncs_2passes++;
	}
	endsearch = roundup(dp->i_size, DIRBLKSIZ);
	enduseful = 0;

searchloop:
	while (ndp->ni_ufs.ufs_offset < endsearch) {
		/*
		 * If offset is on a block boundary,
		 * read the next directory block.
		 * Release previous if it exists.
		オフセットがブロック境界上にある場合は、次のディレクトリブロックを
		読み取ります。前のブロックが存在する場合は解放します。
		 */
		if (blkoff(fs, ndp->ni_ufs.ufs_offset) == 0) {
			if (bp != NULL)
				brelse(bp);
			error = blkatoff(dp, ndp->ni_ufs.ufs_offset, (char **)0, &bp);
			if (error != 0)
				return (error);
			entryoffsetinblock = 0;
		}
		/*
		 * If still looking for a slot, and at a DIRBLKSIZE
		 * boundary, have to start looking for free space again.
		 まだスロットを探していて、DIRBLKSIZE 境界にある場合は、空き領域を
		 再度探し始める必要があります。
		 */
		if (slotstatus == NONE &&
		    (entryoffsetinblock & (DIRBLKSIZ - 1)) == 0) {
			slotoffset = -1;
			slotfreespace = 0;
		}
		/*
		 * Get pointer to next entry.
		 * Full validation checks are slow, so we only check
		 * enough to insure forward progress through the
		 * directory. Complete checks can be run by patching
		 * "dirchk" to be true.
		次のエントリへのポインタを取得します。完全な検証チェックは時間がか
		かるため、ディレクトリ内の前進を確実にするために必要なチェックのみ
		を行います。完全なチェックは、"dirchk" を true にパッチすることで
		実行できます。
		 */
		ep = (struct direct *)(bp->b_un.b_addr + entryoffsetinblock);
		if (ep->d_reclen == 0 ||
		    dirchk && dirbadentry(ep, entryoffsetinblock)) {
			int i;

			dirbad(dp, ndp->ni_ufs.ufs_offset, "mangled entry");
			i = DIRBLKSIZ - (entryoffsetinblock & (DIRBLKSIZ - 1));
			ndp->ni_ufs.ufs_offset += i;
			entryoffsetinblock += i;
			continue;
		}

		/*
		 * If an appropriate sized slot has not yet been found,
		 * check to see if one is available. Also accumulate space
		 * in the current block so that we can determine if
		 * compaction is viable.
		適切なサイズのスロットがまだ見つからない場合は、空きがあるかどうかを
		確認します。また、現在のブロックにスペースを蓄積して、圧縮が実行可能
		かどうかを判断します。
		 */
		if (slotstatus != FOUND) {
			int size = ep->d_reclen;

			if (ep->d_ino != 0)
				size -= DIRSIZ(ep);
			if (size > 0) {
				if (size >= slotneeded) {
					slotstatus = FOUND;
					slotoffset = ndp->ni_ufs.ufs_offset;
					slotsize = ep->d_reclen;
				} else if (slotstatus == NONE) {
					slotfreespace += size;
					if (slotoffset == -1)
						slotoffset =
						      ndp->ni_ufs.ufs_offset;
					if (slotfreespace >= slotneeded) {
						slotstatus = COMPACT;
						slotsize =
						      ndp->ni_ufs.ufs_offset +
						      ep->d_reclen - slotoffset;
					}
				}
			}
		}

		/*
		 * Check for a name match.
		 */
		if (ep->d_ino) {
			if (ep->d_namlen == ndp->ni_namelen &&
			    !memcmp(ndp->ni_ptr, ep->d_name,
				(unsigned)ep->d_namlen)) {
				/*
				 * Save directory entry's inode number and
				 * reclen in ndp->ni_ufs area, and release
				 * directory buffer.
				 */
				ndp->ni_ufs.ufs_ino = ep->d_ino;
				ndp->ni_ufs.ufs_reclen = ep->d_reclen;
				goto found;
			}
		}
		prevoff = ndp->ni_ufs.ufs_offset;
		ndp->ni_ufs.ufs_offset += ep->d_reclen;
		entryoffsetinblock += ep->d_reclen;
		if (ep->d_ino)
			enduseful = ndp->ni_ufs.ufs_offset;
	}
/* notfound: */
	/*
	 * If we started in the middle of the directory and failed
	 * to find our target, we must check the beginning as well.
	 */
	if (numdirpasses == 2) {
		numdirpasses--;
		ndp->ni_ufs.ufs_offset = 0;
		endsearch = dp->i_diroff;
		goto searchloop;
	}
	if (bp != NULL)
		brelse(bp);
	/*
	 * If creating, and at end of pathname and current
	 * directory has not been removed, then can consider
	 * allowing file to be created.
	 */
	if ((flag == CREATE || flag == RENAME) &&
	    *ndp->ni_next == 0 && dp->i_nlink != 0) {
		/*
		 * Access for write is interpreted as allowing
		 * creation of files in the directory.
		 */
		error = ufs_access(vdp, VWRITE, ndp->ni_cred, p);
		if (error != 0)
			return (error);
		/*
		 * Return an indication of where the new directory
		 * entry should be put.  If we didn't find a slot,
		 * then set ndp->ni_ufs.ufs_count to 0 indicating
		 * that the new slot belongs at the end of the
		 * directory. If we found a slot, then the new entry
		 * can be put in the range from ndp->ni_ufs.ufs_offset
		 * to ndp->ni_ufs.ufs_offset + ndp->ni_ufs.ufs_count.
		 */
		if (slotstatus == NONE) {
			ndp->ni_ufs.ufs_offset = roundup(dp->i_size, DIRBLKSIZ);
			ndp->ni_ufs.ufs_count = 0;
			enduseful = ndp->ni_ufs.ufs_offset;
		} else {
			ndp->ni_ufs.ufs_offset = slotoffset;
			ndp->ni_ufs.ufs_count = slotsize;
			if (enduseful < slotoffset + slotsize)
				enduseful = slotoffset + slotsize;
		}
		ndp->ni_ufs.ufs_endoff = roundup(enduseful, DIRBLKSIZ);
		dp->i_flag |= IUPD|ICHG;
		/*
		 * We return with the directory locked, so that
		 * the parameters we set up above will still be
		 * valid if we actually decide to do a direnter().
		 * We return ni_vp == NULL to indicate that the entry
		 * does not currently exist; we leave a pointer to
		 * the (locked) directory inode in ndp->ni_dvp.
		 * The pathname buffer is saved so that the name
		 * can be obtained later.
		 *
		 * NB - if the directory is unlocked, then this
		 * information cannot be used.
		 */
		ndp->ni_nameiop |= SAVENAME;
		if (!lockparent)
			iunlock(dp);
	}
	/*
	 * Insert name into cache (as non-existent) if appropriate.
	 */
	if (ndp->ni_makeentry && flag != CREATE)
		cache_enter(ndp);
	return (ENOENT);

found:
	if (numdirpasses == 2)
		nchstats.ncs_pass2++;
	/*
	 * Check that directory length properly reflects presence
	 * of this entry.
	 */
	if (entryoffsetinblock + DIRSIZ(ep) > dp->i_size) {
		dirbad(dp, ndp->ni_ufs.ufs_offset, "i_size too small");
		dp->i_size = entryoffsetinblock + DIRSIZ(ep);
		dp->i_flag |= IUPD|ICHG;
	}

	brelse(bp);

	/*
	 * Found component in pathname.
	 * If the final component of path name, save information
	 * in the cache as to where the entry was found.
	 */
	if (*ndp->ni_next == '\0' && flag == LOOKUP)
		dp->i_diroff = ndp->ni_ufs.ufs_offset &~ (DIRBLKSIZ - 1);

	/*
	 * If deleting, and at end of pathname, return
	 * parameters which can be used to remove file.
	 * If the wantparent flag isn't set, we return only
	 * the directory (in ndp->ni_dvp), otherwise we go
	 * on and lock the inode, being careful with ".".
	 */
	if (flag == DELETE && *ndp->ni_next == 0) {
		/*
		 * Write access to directory required to delete files.
		 */
		error = ufs_access(vdp, VWRITE, ndp->ni_cred, p);
		if (error != 0)
			return (error);
		/*
		 * Return pointer to current entry in ndp->ni_ufs.ufs_offset,
		 * and distance past previous entry (if there
		 * is a previous entry in this block) in ndp->ni_ufs.ufs_count.
		 * Save directory inode pointer in ndp->ni_dvp for dirremove().
		 */
		if ((ndp->ni_ufs.ufs_offset&(DIRBLKSIZ-1)) == 0)
			ndp->ni_ufs.ufs_count = 0;
		else
			ndp->ni_ufs.ufs_count = ndp->ni_ufs.ufs_offset - prevoff;
		if (dp->i_number == ndp->ni_ufs.ufs_ino) {
			VREF(vdp);
			ndp->ni_vp = vdp;
			return (0);
		}
		error = iget(dp, ndp->ni_ufs.ufs_ino, &tdp);
		if (error != 0)
			return (error);
		/*
		 * If directory is "sticky", then user must own
		 * the directory, or the file in it, else she
		 * may not delete it (unless she's root). This
		 * implements append-only directories.
		 */
		if ((dp->i_mode & ISVTX) &&
		    ndp->ni_cred->cr_uid != 0 &&
		    ndp->ni_cred->cr_uid != dp->i_uid &&
		    tdp->i_uid != ndp->ni_cred->cr_uid) {
			iput(tdp);
			return (EPERM);
		}
		ndp->ni_vp = ITOV(tdp);
		if (!lockparent)
			iunlock(dp);
		return (0);
	}

	/*
	 * If rewriting (RENAME), return the inode and the
	 * information required to rewrite the present directory
	 * Must get inode of directory entry to verify it's a
	 * regular file, or empty directory.
	 */
	if (flag == RENAME && wantparent && *ndp->ni_next == 0) {
		error = ufs_access(vdp, VWRITE, ndp->ni_cred, p);
		if (error != 0)
		{
			return (error);
		}
		/*
		 * Careful about locking second inode.
		 * This can only occur if the target is ".".
		 */
		if (dp->i_number == ndp->ni_ufs.ufs_ino)
			return (EISDIR);
		error = iget(dp, ndp->ni_ufs.ufs_ino, &tdp);
		if (error != 0)
			return (error);
		ndp->ni_vp = ITOV(tdp);
		ndp->ni_nameiop |= SAVENAME;
		if (!lockparent)
			iunlock(dp);
		return (0);
	}

	/*
	 * Step through the translation in the name.  We do not `iput' the
	 * directory because we may need it again if a symbolic link
	 * is relative to the current directory.  Instead we save it
	 * unlocked as "pdp".  We must get the target inode before unlocking
	 * the directory to insure that the inode will not be removed
	 * before we get it.  We prevent deadlock by always fetching
	 * inodes from the root, moving down the directory tree. Thus
	 * when following backward pointers ".." we must unlock the
	 * parent directory before getting the requested directory.
	 * There is a potential race condition here if both the current
	 * and parent directories are removed before the `iget' for the
	 * inode associated with ".." returns.  We hope that this occurs
	 * infrequently since we cannot avoid this race condition without
	 * implementing a sophisticated deadlock detection algorithm.
	 * Note also that this simple deadlock detection scheme will not
	 * work if the file system has any hard links other than ".."
	 * that point backwards in the directory structure.
	 */
	pdp = dp;
	if (ndp->ni_isdotdot) {
		iunlock(pdp);	/* race to get the inode */
		error = iget(dp, ndp->ni_ufs.ufs_ino, &tdp);
		if (error != 0) {
			ilock(pdp);
			return (error);
		}
		if (lockparent && *ndp->ni_next == '\0')
			ilock(pdp);
		ndp->ni_vp = ITOV(tdp);
	} else if (dp->i_number == ndp->ni_ufs.ufs_ino) {
		VREF(vdp);	/* we want ourself, ie "." */
		ndp->ni_vp = vdp;
	} else {
		error = iget(dp, ndp->ni_ufs.ufs_ino, &tdp);
		if (error != 0)
			return (error);
		if (!lockparent || *ndp->ni_next != '\0')
			iunlock(pdp);
		ndp->ni_vp = ITOV(tdp);
	}

	/*
	 * Insert name into cache if appropriate.
	 */
	if (ndp->ni_makeentry)
		cache_enter(ndp);
	return (0);
}


void dirbad(struct inode* ip, off_t offset, char* how)
{

	printf("%s: bad dir ino %d at offset %d: %s\n",
	    ip->i_fs->fs_fsmnt, ip->i_number, offset, how);
	if (ip->i_fs->fs_ronly == 0)
		panic("bad dir");
}

/*
 * Do consistency checking on a directory entry:
 *	record length must be multiple of 4
 *	entry must fit in rest of its DIRBLKSIZ block
 *	record must be large enough to contain entry
 *	name is not longer than MAXNAMLEN
 *	name must be as long as advertised, and null terminated
 */
static int dirbadentry(register struct direct*ep, int entryoffsetinblock)
{
	register int i;

	if ((ep->d_reclen & 0x3) != 0 ||
	    ep->d_reclen > DIRBLKSIZ - (entryoffsetinblock & (DIRBLKSIZ - 1)) ||
	    ep->d_reclen < DIRSIZ(ep) || ep->d_namlen > MAXNAMLEN)
		return (1);
	for (i = 0; i < ep->d_namlen; i++)
		if (ep->d_name[i] == '\0')
			return (1);
	return (ep->d_name[i]);
}

/*
 * Write a directory entry after a call to namei, using the parameters
 * that it left in nameidata.  The argument ip is the inode which the new
 * directory entry will refer to.  The nameidata field ndp->ni_dvp is a
 * pointer to the directory to be written, which was left locked by namei.
 * Remaining parameters (ndp->ni_ufs.ufs_offset, ndp->ni_ufs.ufs_count)
 * indicate how the space for the new entry is to be obtained.
 */
int direnter(struct inode* ip, register struct nameidata* ndp)
{
	register struct direct *ep, *nep;
	register struct inode *dp = VTOI(ndp->ni_dvp);
	struct buf *bp;
	int loc, spacefree, error = 0;
	u_int dsize;
	int newentrysize;
	char *dirbuf;
	struct uio auio;
	struct iovec aiov;
	struct direct newdir;

#ifdef DIAGNOSTIC
	if ((ndp->ni_nameiop & SAVENAME) == 0)
		panic("direnter: missing name");
#endif
	newdir.d_ino = ip->i_number;
	newdir.d_namlen = ndp->ni_namelen;
	memcpy(newdir.d_name, ndp->ni_ptr, (unsigned)ndp->ni_namelen + 1);
	newdir.d_type = (ndp->ni_dvp->v_mount->mnt_maxsymlinklen > 0) ? 
					IFTODT(ip->i_mode) : 0;
	newentrysize = DIRSIZ(&newdir);
	if (ndp->ni_ufs.ufs_count == 0) {
		/*
		 * If ndp->ni_ufs.ufs_count is 0, then namei could find no
		 * space in the directory. Here, ndp->ni_ufs.ufs_offset will
		 * be on a directory block boundary and we will write the
		 * new entry into a fresh block.
		 */
		if (ndp->ni_ufs.ufs_offset & (DIRBLKSIZ - 1))
			panic("wdir: newblk");
		auio.uio_offset = ndp->ni_ufs.ufs_offset;
		newdir.d_reclen = DIRBLKSIZ;
		auio.uio_resid = newentrysize;
		aiov.iov_len = newentrysize;
		aiov.iov_base = (caddr_t)&newdir;
		auio.uio_iov = &aiov;
		auio.uio_iovcnt = 1;
		auio.uio_rw = UIO_WRITE;
		auio.uio_segflg = UIO_SYSSPACE;
		auio.uio_procp = (struct proc *)0;
		error = ufs_write(ndp->ni_dvp, &auio, IO_SYNC, ndp->ni_cred);
		if (DIRBLKSIZ > dp->i_fs->fs_fsize) {
			panic("wdir: blksize"); /* XXX - should grow w/balloc */
		} else if (!error) {
			dp->i_size = roundup(dp->i_size, DIRBLKSIZ);
			dp->i_flag |= ICHG;
		}
		return (error);
	}

	/*
	 * If ndp->ni_ufs.ufs_count is non-zero, then namei found space
	 * for the new entry in the range ndp->ni_ufs.ufs_offset to
	 * ndp->ni_ufs.ufs_offset + ndp->ni_ufs.ufs_count in the directory.
	 * To use this space, we may have to compact the entries located
	 * there, by copying them together towards the beginning of the
	 * block, leaving the free space in one usable chunk at the end.
	 */

	/*
	 * Increase size of directory if entry eats into new space.
	 * This should never push the size past a new multiple of
	 * DIRBLKSIZE.
	 *
	 * N.B. - THIS IS AN ARTIFACT OF 4.2 AND SHOULD NEVER HAPPEN.
	 */
	if (ndp->ni_ufs.ufs_offset + ndp->ni_ufs.ufs_count > dp->i_size)
		dp->i_size = ndp->ni_ufs.ufs_offset + ndp->ni_ufs.ufs_count;
	/*
	 * Get the block containing the space for the new directory entry.
	 */
	error = blkatoff(dp, ndp->ni_ufs.ufs_offset, (char **)&dirbuf, &bp);
	if (error != 0)
		return (error);
	/*
	 * Find space for the new entry. In the simple case, the entry at
	 * offset base will have the space. If it does not, then namei
	 * arranged that compacting the region ndp->ni_ufs.ufs_offset to
	 * ndp->ni_ufs.ufs_offset + ndp->ni_ufs.ufs_count would yield the
	 * space.
	 */
	ep = (struct direct *)dirbuf;
	dsize = DIRSIZ(ep);
	spacefree = ep->d_reclen - dsize;
	for (loc = ep->d_reclen; loc < ndp->ni_ufs.ufs_count; ) {
		nep = (struct direct *)(dirbuf + loc);
		if (ep->d_ino) {
			/* trim the existing slot */
			ep->d_reclen = dsize;
			ep = (struct direct *)((char *)ep + dsize);
		} else {
			/* overwrite; nothing there; header is ours */
			spacefree += dsize;
		}
		dsize = DIRSIZ(nep);
		spacefree += nep->d_reclen - dsize;
		loc += nep->d_reclen;
/* XXX!*/	memcpy((caddr_t)ep, (caddr_t)nep, dsize);
	}
	/*
	 * Update the pointer fields in the previous entry (if any),
	 * copy in the new entry, and write out the block.
	 */
	if (ep->d_ino == 0) {
		if (spacefree + dsize < newentrysize)
			panic("wdir: compact1");
		newdir.d_reclen = spacefree + dsize;
	} else {
		if (spacefree < newentrysize)
			panic("wdir: compact2");
		newdir.d_reclen = spacefree;
		ep->d_reclen = dsize;
		ep = (struct direct *)((char *)ep + dsize);
	}
	memcpy((caddr_t)ep, (caddr_t)&newdir, (u_int)newentrysize);
#ifdef was
	error = bwrite(bp);
#else
#define	bowrite(b)	{ (b)->b_flags |= B_ORDER; bawrite((b)); }
	error = 0;
	bowrite(bp);
#endif
	dp->i_flag |= IUPD|ICHG;
	if (!error && ndp->ni_ufs.ufs_endoff &&
	    ndp->ni_ufs.ufs_endoff < dp->i_size)
		error = itrunc(dp, (u_long)ndp->ni_ufs.ufs_endoff, IO_SYNC);
	return (error);
}

/*
 * Remove a directory entry after a call to namei, using
 * the parameters which it left in nameidata. The entry
 * ni_ufs.ufs_offset contains the offset into the directory of the
 * entry to be eliminated.  The ni_ufs.ufs_count field contains the
 * size of the previous record in the directory.  If this
 * is 0, the first entry is being deleted, so we need only
 * zero the inode number to mark the entry as free.  If the
 * entry is not the first in the directory, we must reclaim
 * the space of the now empty record by adding the record size
 * to the size of the previous entry.
 */
int dirremove(register struct nameidata *ndp)
{
	register struct inode *dp = VTOI(ndp->ni_dvp);
	struct direct *ep;
	struct buf *bp;
	int error;

	if (ndp->ni_ufs.ufs_count == 0) {
		/*
		 * First entry in block: set d_ino to zero.
		 */
		error = blkatoff(dp, ndp->ni_ufs.ufs_offset, (char **)&ep, &bp);
		if (error)
			return (error);
		ep->d_ino = 0;
//		ep->d_ino = WINO
		ep->d_type = DT_WHT;
#ifdef was
		error = bwrite(bp);
#else
		error = 0;
		bowrite(bp);
#endif
		dp->i_flag |= IUPD|ICHG;
		return (error);
	}
	/*
	 * Collapse new free space into previous entry.
	 */
	error = blkatoff(dp, ndp->ni_ufs.ufs_offset - ndp->ni_ufs.ufs_count,
	    (char **)&ep, &bp);
	if (error != 0) {
		return (error);
	}
	ep->d_reclen += ndp->ni_ufs.ufs_reclen;
#ifdef nope
	error = bwrite(bp);
#else
		error = 0;
		bowrite(bp);
#endif
	dp->i_flag |= IUPD|ICHG;
	return (error);
}

/*
 * Rewrite an existing directory entry to point at the inode
 * supplied.  The parameters describing the directory entry are
 * set up by a call to namei.
 */
int dirrewrite(struct inode* dp, struct inode* ip, struct nameidata* ndp)
{
	struct direct *ep;
	struct buf *bp;
	int error;

	error = blkatoff(dp, ndp->ni_ufs.ufs_offset, (char **)&ep, &bp);
	if (error != 0)
		return (error);
	ep->d_ino = ip->i_number;
	ep->d_type = (dp->i_vnode->v_mount->mnt_maxsymlinklen > 0) ? 
				 IFTODT(ip->i_mode) : 0;
#ifdef was
	error = bwrite(bp);
#else
	error = 0;
	bowrite(bp);
#endif
	dp->i_flag |= IUPD|ICHG;
	return (error);
}

/*
 * Return buffer with contents of block "offset"
 * from the beginning of directory "ip".  If "res"
 * is non-zero, fill it in with a pointer to the
 * remaining space in the directory.
 */
static int blkatoff(struct inode* ip, off_t offset, char** res, struct buf** bpp)
{
	register struct fs *fs = ip->i_fs;
	daddr_t lbn = lblkno(fs, offset);
	int bsize = blksize(fs, ip, lbn);
	struct buf *bp;
	daddr_t bn;
	int error;

	*bpp = 0;
	error = bread(ITOV(ip), lbn, bsize, NOCRED, &bp);
	if (error != 0) {
		brelse(bp);
		return (error);
	}
	if (res)
		*res = bp->b_un.b_addr + blkoff(fs, offset);
	*bpp = bp;
	return (0);
}

/*
 * Check if a directory is empty or not.
 * Inode supplied must be locked.
 *
 * Using a struct dirtemplate here is not precisely
 * what we want, but better than using a struct direct.
 *
 * NB: does not handle corrupted directories.
 */
int dirempty(register struct inode* ip, ino_t parentino, struct ucred* cred)
{
	register off_t off;
	struct dirtemplate dbuf;
	register struct direct *dp = (struct direct *)&dbuf;
	int error, count;
#define	MINDIRSIZ (sizeof (struct dirtemplate) / 2)

	for (off = 0; off < ip->i_size; off += dp->d_reclen) {
		error = vn_rdwr(UIO_READ, ITOV(ip), (caddr_t)dp, MINDIRSIZ, off,
		   UIO_SYSSPACE, IO_NODELOCKED, cred, &count, (struct proc *)0);
		/*
		 * Since we read MINDIRSIZ, residual must
		 * be 0 unless we're at end of file.
		 */
		if (error || count != 0)
			return (0);
		/* avoid infinite loops */
		if (dp->d_reclen == 0)
			return (0);
		/* skip empty entries */
		if (dp->d_ino == 0)
			continue;
		/* accept only "." and ".." */
		if (dp->d_namlen > 2)
			return (0);
		if (dp->d_name[0] != '.')
			return (0);
		/*
		 * At this point d_namlen must be 1 or 2.
		 * 1 implies ".", 2 implies ".." if second
		 * char is also "."
		 */
		if (dp->d_namlen == 1)
			continue;
		if (dp->d_name[1] == '.' && dp->d_ino == parentino)
			continue;
		return (0);
	}
	return (1);
}

/*
 * Check if source directory is in the path of the target directory.
 * Target is supplied locked, source is unlocked.
 * The target is always iput() before returning.
 */
int checkpath(struct inode* source,struct inode* target, struct ucred* cred)
{
	struct dirtemplate dirbuf;
	struct inode *ip;
	int error = 0;

	ip = target;
	if (ip->i_number == source->i_number) {
		error = EEXIST;
		goto out;
	}
	if (ip->i_number == ROOTINO)
		goto out;

	for (;;) {
		if ((ip->i_mode&IFMT) != IFDIR) {
			error = ENOTDIR;
			break;
		}
		error = vn_rdwr(UIO_READ, ITOV(ip), (caddr_t)&dirbuf,
			sizeof (struct dirtemplate), (off_t)0, UIO_SYSSPACE,
			IO_NODELOCKED, cred, (int *)0, (struct proc *)0);
		if (error != 0)
			break;
		if (dirbuf.dotdot_namlen != 2 ||
		    dirbuf.dotdot_name[0] != '.' ||
		    dirbuf.dotdot_name[1] != '.') {
			error = ENOTDIR;
			break;
		}
		if (dirbuf.dotdot_ino == source->i_number) {
			error = EINVAL;
			break;
		}
		if (dirbuf.dotdot_ino == ROOTINO)
			break;
		iput(ip);
		error = iget(ip, dirbuf.dotdot_ino, &ip);
		if (error != 0)
			break;
	}

out:
	if (error == ENOTDIR)
		printf("checkpath: .. not a directory\n");
	if (ip != NULL)
		iput(ip);
	return (error);
}
