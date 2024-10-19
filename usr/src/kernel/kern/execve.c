/*
 * Copyright (c) 1989, 1990, 1991, 1992, 1994 William F. Jolitz, TeleMuse
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
 *	William F. Jolitz, TeleMuse.
 * 4. Neither the name of the developer nor the name "386BSD"
 *    may be used to endorse or promote products derived from this software
 *    without specific prior written permission.
 * 5. Non-commercial distribution of this complete file in either source and/or
 *    binary form at no charge to the user (such as from an official Internet 
 *    archive site) is permitted. 
 * 6. Commercial distribution and sale of this complete file in either source
 *    and/or binary form on any media, including that of floppies, tape, or 
 *    CD-ROM, or through a per-charge download such as that of a BBS, is not 
 *    permitted without specific prior written permission.
 * 7. Non-commercial and/or commercial distribution of an incomplete, altered, 
 *    or otherwise modified file in either source and/or binary form is not 
 *    permitted.
 *
 * THIS SOFTWARE IS A COMPONENT OF 386BSD DEVELOPED BY WILLIAM F. JOLITZ 
 * AND IS INTENDED FOR RESEARCH AND EDUCATIONAL PURPOSES ONLY. THIS 
 * SOFTWARE SHOULD NOT BE CONSIDERED TO BE A COMMERCIAL PRODUCT. 
 * THE DEVELOPER URGES THAT USERS WHO REQUIRE A COMMERCIAL PRODUCT 
 * NOT MAKE USE OF THIS WORK.
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
 * $Id: execve.c,v 1.1 94/10/20 00:02:51 bill Exp Locker: bill $
 *
 * This procedure implements a minimal program execution facility for
 * 386BSD. It interfaces to the BSD kernel as the execve() system call.
 */

#include "sys/param.h"
#include "sys/file.h"
#include "sys/mount.h"
#include "sys/exec.h"
#include "sys/stat.h"
#include "sys/wait.h"
#include "sys/mman.h"
#include "sys/errno.h"

#include "malloc.h"
#include "systm.h"
#include "proc.h"
#include "resourcevar.h"
#include "uio.h"
#include "vm.h"
#include "vmspace.h"
#include "vm_pager.h"		/* XXX */

#include "namei.h"
#include "vnode.h"

#undef	KERNEL			/* XXX */
#include "vnode_pager.h"	/* XXX */
#define	KERNEL

#include "machine/reg.h"
#include "machine/cpu.h"

#include "prototypes.h"

#define	copyinoutstr	copyinstr

struct execve_args
{
	char	*fname;
	char	**argp;
	char	**envp;
};

struct execv_params
{
	struct nameidata	nd;
	struct exec 		hdr;
	struct vattr		attr;
};

#define ELF32_NO_ADDR	(~(Elf32_Addr)0)
#define ELF32_LINK_ADDR ((Elf32_Addr)-2) /* advises to use link address */

#define	ON_ERROR_GOTO(a, b)	if (a) { goto b; }

extern void execsigs(struct proc *p);
extern int 	vmspace_mmap(struct vmspace *vs, vm_offset_t *addr,
						 vm_size_t size, vm_prot_t prot, int flags,
						 caddr_t handle, vm_offset_t foff);
extern void psignal(struct proc *p, int sig);
extern void fdcloseexec(struct proc *p);
extern int 	chk4space(int pages);

static int execv_aout(struct proc* p, struct execv_params* eprms, struct execve_args* uap);
static int execv_elf32(struct proc* p, struct execv_params* eprms, struct execve_args* uap);
static int exec_read(struct proc* p, struct vnode* vp, u_long off,
					 void* bf, int size, int ioflg);
static int alloc_new_stack(struct proc* p, struct execve_args* uap, char*** argbuf);


static int exec_read(struct proc* p, struct vnode* vp, u_long off,
					 void* bf, int size, int ioflg)
{
	int		error;
	int		resid;

	error = vn_rdwr(UIO_READ, vp, bf, size, off, UIO_SYSSPACE, ioflg,
					p->p_ucred, &resid, NULL);
	if (error != 0)
	{
		return error;
	}

	if (resid != 0)
	{
		return ENOEXEC;
	}

	return (0);
}

static int check_execv(struct proc* p, struct execv_params* eprms)
{
	struct nameidata *ndp;
	int	rv;

	/*
	 * Step 1. Lookup filename to see if we have something to execute.
	 */
	ndp = &eprms->nd;

	/* is this a valid vnode representing a possibly executable file ? */
	if (ndp->ni_vp->v_type != VREG)
	{
		return EACCES;
	}

	/* does the filesystem it belongs to allow files to be executed? */
	if ((ndp->ni_vp->v_mount->mnt_flag & MNT_NOEXEC) != 0)
	{
		return EACCES;
	}

	/* does it have any attributes? */
	rv = VOP_GETATTR(ndp->ni_vp, &eprms->attr, p->p_ucred, p);
	if (rv != 0)
	{
		return (rv);
	}

	/* is it an executable file? */
	if ((eprms->attr.va_mode & VANYEXECMASK) == 0)
	{
		return EACCES;
	}

	return (0);
}

static int alloc_new_stack(struct proc* p, struct execve_args* uap, char*** argbuf)
{
	struct vmspace*		vs = p->p_vmspace;
	char**				argbufp;
	char*				stringbuf;
	char*				stringbufp;
	char**				vectp;
	char*				ep;
	int					needsenv;
	int					limitonargs;
	int					stringlen;
	int					rv;
	int					argc;
	int					cnt;
	unsigned int		oss, nss;
	vm_offset_t			newframe;

	/* can we fit new stack above old one */
	nss = round_page(3 * ARG_MAX + NBPG);
	if ((unsigned)vs->vm_maxsaddr + MAXSSIZ + nss < USRSTACK)
	{
		newframe = USRSTACK - nss;
	}
	else
	{
		newframe = ((unsigned)vs->vm_maxsaddr + MAXSSIZ - (vs->vm_ssize * NBPG) - nss);
	}

	/* allocate anonymous memory region for new stack, disable stk limit */
	rv = vmspace_allocate(vs, &newframe, nss, FALSE);
	if (rv != 0)
	{
		return ENOMEM;
	}

	oss = vs->vm_ssize;
	vs->vm_ssize = 0;

	/* allocate string buffer and arg buffer */
	*argbuf = (char **) (newframe + NBPG);
	stringbuf = stringbufp = ((char *)*argbuf) + 2 * ARG_MAX;
	argbufp = *argbuf;

	/* first, do the args */
	vectp = uap->argp;
	needsenv = 1;
	limitonargs = ARG_MAX;
	cnt = 0;

do_env_as_well:
	if (vectp == 0)
	{
		goto dont_bother;
	}

	/* for each argp/envp, copy in string */
	do
	{
		/* did we outgrow initial argbuf, if so, die */
		if (argbufp == (char **)stringbuf)
		{
			rv = E2BIG;
			goto exec_dealloc;
		}
	
		/* get an string pointer */
		rv = copyin_(p, (void *)vectp++, (void *)&ep, 4);
		if (rv != 0)
		{
			goto exec_dealloc; 
		}
		/* if not a null pointer, copy string */
		if (ep != 0)
		{
			rv = copyinoutstr(p, ep, stringbufp, (u_int)limitonargs, (u_int *) &stringlen);
			if (rv != 0)
			{
				if (rv == ENAMETOOLONG)
				{
					rv = E2BIG;
				}
				goto exec_dealloc;
			}

			rv = copyout_(p, &stringbufp, (void *)argbufp++, 4);
			if (rv != 0)
			{
				goto exec_dealloc; 
			}
			cnt++;
			stringbufp += stringlen;
			limitonargs -= stringlen;
		}
		else
		{
			rv = copyout_(p, &ep, (void *)argbufp++, 4);
			if (rv != 0)
			{
				goto exec_dealloc; 
			}
			break;
		}
	}
	while (limitonargs > 0);

dont_bother:
	if (limitonargs <= 0)
	{
		rv = E2BIG;
		goto exec_dealloc;
	}

	/* have we done the environment yet ? */
	if (needsenv)
	{
		/* remember the arg count for later */
		argc = cnt;
		vectp = uap->envp;
		needsenv = 0;
		goto do_env_as_well;
	}

	/* stuff arg count on top of "new" stack */
	vs->vm_ssize =  btoc(nss);	/* stack size (pages) XXX */
	vs->vm_maxsaddr = (caddr_t)((unsigned)newframe + nss - MAXSSIZ);
	(*argbuf)--;
//	(void)copyout_(p, &argc, (void *)*argbuf, 4);
	rv = copyout_(p, &argc, (void *)*argbuf, 4);
	if (rv != 0)
	{
		goto exec_dealloc; 
	}

	/* remove anything in address space above our stack */
	if ((unsigned)newframe + nss < VM_MAX_ADDRESS)
	{
		vmspace_delete(vs, (caddr_t)(newframe + nss),
					   VM_MAX_ADDRESS - (unsigned)(newframe) - nss);
	}

	/* blow away all address space, except the stack */
	vmspace_delete(vs, (caddr_t)VM_MIN_ADDRESS, newframe);

	return 0;

exec_dealloc:
	/* remove interim "new" stack frame we were building */
	vmspace_delete(vs, (caddr_t)newframe, nss);

	/* reengauge stack limits */
	vs->vm_ssize =  oss;

	return (rv);
}

static void prepare_proc(struct proc* p, struct execv_params* eprms, caddr_t entry, caddr_t argbuf)
{
	struct nameidata* 	ndp;

	ndp = &eprms->nd;

	/* close files on exec, fixup signals, name this process */
	fdcloseexec(p);
	execsigs(p);
	nameiexec(ndp, p);

	/*
	 * mark as having had an execve(), wakeup any process that was
	 * vforked and tell it that it now has it's own resources back. XXX
	 */
	p->p_flag |= SEXEC;
	if (p->p_pptr && (p->p_flag & SPPWAIT))
	{
		p->p_flag &= ~SPPWAIT;
		wakeup((caddr_t)p->p_pptr);
	}
	
	/* implement set userid/groupid */
	if ((eprms->attr.va_mode&VSUID) && (p->p_flag & STRC) == 0)
	{
		p->p_cred = modpcred(p);
		p->p_cred->p_svuid = p->p_ucred->cr_uid = eprms->attr.va_uid;
	}
	if ((eprms->attr.va_mode&VSGID) && (p->p_flag & STRC) == 0)
	{
		p->p_cred = modpcred(p);
		p->p_cred->p_svgid = p->p_ucred->cr_groups[0] = eprms->attr.va_gid;
	}
	/* setup initial register state */
	cpu_execsetregs(p, entry, argbuf);

	/* if tracing process, pass control back to debugger so
	   breakpoints can be set before the program "runs" */
	if (p->p_flag & STRC)
	{
		psignal(p, SIGTRAP);
	}
}

static int load_aout(struct proc* p, struct execv_params* eprms)
{
	struct nameidata* 	ndp;
	struct vmspace*		vs = p->p_vmspace;
	int					rv, foff;
	vm_offset_t			addr;
	vm_offset_t			taddr;
	vm_offset_t			daddr;
	vm_size_t			tsize;
	vm_size_t			dsize;
	vm_size_t			bsize;

	ndp = &eprms->nd;

	/* build a new address space */
	addr = 0;

	/* screwball mode -- special case of 413 to save space for floppy */
	if (eprms->hdr.a_text == 0)
	{
		foff = tsize = 0;
	}
	else
	{
		tsize = roundup(eprms->hdr.a_text, NBPG);
		foff = NBPG;
	}

	/* treat text and data in terms of integral page size */
	dsize = roundup(eprms->hdr.a_data, NBPG);
	bsize = roundup(eprms->hdr.a_bss + dsize, NBPG);
	bsize -= dsize;

	/* map text & data in file, as being "paged in" on demand */
	rv = vmspace_mmap(vs, &addr, tsize + dsize, VM_PROT_ALL,
					  MAP_FILE|MAP_COPY, (caddr_t)ndp->ni_vp, foff);
	if (rv != 0)
	{
		return rv;
	}

	/* mark pages r/w data, r/o text */
	if (tsize != 0)
	{
		addr = 0;
		rv = vmspace_protect(vs, (caddr_t)addr, tsize, FALSE,
							 VM_PROT_READ|VM_PROT_EXECUTE);
		if (rv != 0)
		{
		return rv;
		}
	}

	/* create anonymous memory region for bss */
	addr = dsize + tsize;
	rv = vmspace_allocate(vs, &addr, bsize, FALSE);
	if (rv != 0)
	{
		return rv;
	}

	/* touchup process information -- vm system is unfinished! */
	vs->vm_tsize = tsize / NBPG;			/* text size (pages) XXX */
	vs->vm_dsize = (dsize + bsize) / NBPG;	/* data size (pages) XXX */
	vs->vm_taddr = 0;						/* user virtual address of text XXX */
	vs->vm_daddr = (caddr_t)tsize;			/* user virtual address of data XXX */
//printf("tsize = %x, dsize = %x, bsize = %x, vs->vm_dsize = %x, vs->vm_daddr = %x\n", tsize, dsize, bsize, vs->vm_dsize, vs->vm_daddr);

	return 0;
}

static int check_aout_hdr(struct proc* p, struct execv_params* eprms)
{
	int					rv;

	/* sanity check  "ain't not such thing as a sanity clause" -groucho */
	if (eprms->hdr.a_text > MAXTSIZ
		|| eprms->hdr.a_text % NBPG
	    || eprms->hdr.a_text > eprms->attr.va_size)
	{
		return (ENOMEM);;
	}

	if (eprms->hdr.a_data == 0
		|| eprms->hdr.a_data > DFLDSIZ
	    || eprms->hdr.a_data > eprms->attr.va_size
	    || eprms->hdr.a_data + eprms->hdr.a_text > eprms->attr.va_size)
	{
		return (ENOMEM);;
	}

	if (eprms->hdr.a_bss > MAXDSIZ)
	{
		return (ENOMEM);;
	}

	if (eprms->hdr.a_text + eprms->hdr.a_data + eprms->hdr.a_bss > MAXTSIZ + MAXDSIZ)
	{
		return (ENOMEM);;
	}

	if (eprms->hdr.a_entry > eprms->hdr.a_text + eprms->hdr.a_data)
	{
		return (ENOMEM);;
	}

	if (eprms->hdr.a_data + eprms->hdr.a_bss > p->p_rlimit[RLIMIT_DATA].rlim_cur)
	{
		return (ENOMEM);;
	}

	/* don't allow new process contents to exceed memory resources */
	rv = chk4space(atop(eprms->hdr.a_data + eprms->hdr.a_bss + 3*ARG_MAX));
	if (rv == 0)
	{
		return (rv);;
	}

	return (0);
}

#if 0	// execv_aout
static int execv_aout(struct proc* p, struct execv_params* eprms, struct execve_args* uap)
{
	struct nameidata* 	ndp;
	struct vmspace*		vs = p->p_vmspace;
	char				**argbuf;
	int					rv;

	ndp = &eprms->nd;

	rv = check_aout_hdr(p, eprms);
	if (rv != 0)
	{
		rv = ENOMEM;
		goto exec_fail;
	}

	rv = alloc_new_stack(p, uap, &argbuf);
	if (rv != 0)
	{
		goto exec_fail;
	}

	rv = load_aout(p, eprms);
	if (rv != 0)
	{
		goto exec_abort;
	}

	prepare_proc(p, eprms, (caddr_t)eprms->hdr.a_entry, (caddr_t)argbuf);

	return (0);

exec_fail:
	/* release namei request */
	nameiexec(ndp, (struct proc *)0);

	return(rv);

exec_abort:
	/* sorry, no more process anymore. exit gracefully */

	/* release namei request */
	nameiexec(ndp, (struct proc *)0);

	exit(p, W_EXITCODE(0, SIGABRT));

	/* NOTREACHED */
	return(0);
}
#endif

int vmcmd_map_readvn(struct proc* p, struct vnode* vp,
					 vm_offset_t addr, vm_size_t size,
					 vm_prot_t prot, vm_offset_t offset)
{
	int					rv;

	rv = vmspace_allocate(p->p_vmspace, &addr, size, FALSE);
	if (rv != 0)
	{
		return rv;
	}

	rv = vn_rdwr(UIO_READ, vp, (caddr_t)addr, size, offset,
				 UIO_USERSPACE, IO_UNIT | IO_NODELOCKED, p->p_ucred, (int*)0, p);
	if (rv != 0)
	{
		return rv;
	}

	rv = vmspace_protect(p->p_vmspace, (caddr_t)addr, size, FALSE, prot);

	return rv;
}

int vmcmd_map_zero(struct proc* p, vm_offset_t addr,
				   vm_size_t size, vm_prot_t prot)
{
	int					rv;

	rv = vmspace_allocate(p->p_vmspace, &addr, size, FALSE);
	if (rv != 0)
	{
		return rv;
	}

	rv = vmspace_protect(p->p_vmspace, (caddr_t)addr, size, FALSE, prot);

	return rv;
}

static int load_elf32(struct proc* p, struct execv_params* eprms)
{
	struct nameidata* 	ndp;
	int					rv;
	int					i;
	struct vmspace*		vs = p->p_vmspace;
	vm_size_t			phsize;
	vm_size_t			size;
	Elf32_Addr			last;
	Elf32_Addr			taddr;
	Elf32_Addr			daddr;
	vm_size_t			tsize;
	vm_size_t			dsize;
	vm_size_t			bsize;
	struct exec*		hdr;
	Elf32_Phdr*			phdr;

	ndp = &eprms->nd;
	hdr = &eprms->hdr;
	tsize = dsize = bsize = 0;

	/* プログラムヘッダ用のメモリ確保 */
	phsize = hdr->e_phnum * sizeof(Elf32_Phdr);
	phdr = (Elf32_Phdr*)malloc(phsize, M_TEMP, M_WAITOK);
	if (phdr == NULL)
	{
		return (ENOMEM);
	}

	/* プログラムヘッダの読み込み */
	rv = exec_read(p, ndp->ni_vp, hdr->e_phoff, phdr, phsize, IO_NODELOCKED);
	if (rv != 0)
	{
		rv = ENOMEM;
		goto error;
	}

	/*
		プログラムヘッダーの情報をもとに実行ファイルを読み込み、
	　　スタックなど必要なメモリを確保
	*/
	for (i = 0; i < hdr->e_phnum; i++)
	{
		Elf32_Phdr*		ph = &phdr[i];
		Elf32_Addr		addr, uaddr;
		Elf32_Addr		rm, rf;
		vm_size_t		msize, psize;
		vm_prot_t		prot;
		long			offset;
		long			diff;

		/* タイプがPT_LOAD以外はスルー */
		if (ph->p_type != PT_LOAD)
		{
			continue;
		}

		addr = uaddr = ph->p_vaddr;
		if (ph->p_align > 1)
		{
			addr = ELF_TRUNC(uaddr, ph->p_align);
		}
		diff = uaddr - addr;

		prot = VM_PROT_NONE;
		prot |= (ph->p_flags & PF_R) ? VM_PROT_READ		: 0;
		prot |= (ph->p_flags & PF_W) ? VM_PROT_WRITE	: 0;
		prot |= (ph->p_flags & PF_X) ? VM_PROT_EXECUTE	: 0;

		offset	= ph->p_offset - diff;
		size	= ph->p_filesz + diff;
		msize	= ph->p_memsz  + diff;

		/* セグメントセクションのサイズを0x1000単位で調整 */
		if (ph->p_align >= PAGE_SIZE)
		{
			if ((ph->p_flags & PF_W) != 0)
			{
				psize = trunc_page(size);
			}
			else
			{
				psize = round_page(size);
			}
		}
		else
		{
			psize = size;
		}

		if (psize > 0)
		{
			if (ph->p_align < PAGE_SIZE)
			{
				rv = vmcmd_map_readvn(p, ndp->ni_vp, addr, psize, 
									  prot, offset);
			}
			else
			{
				rv = vmspace_mmap(vs, &addr, psize, prot, MAP_FILE | MAP_COPY,
								  (caddr_t)ndp->ni_vp, offset);
			}
			ON_ERROR_GOTO((rv != 0), error);
		}

		if (psize < size)
		{
			rv = vmcmd_map_readvn(p, ndp->ni_vp, addr + psize, size - psize,
								  prot, offset + psize);
			ON_ERROR_GOTO((rv != 0), error);
		}

		rm = round_page(addr + msize);
		rf = round_page(addr + size);

		if (rm != rf)
		{
			rv = vmcmd_map_zero(p, rf, (vm_size_t)(rm - rf), prot);
			ON_ERROR_GOTO((rv != 0), error);

			size = msize;
		}

		if ((ph->p_flags & PF_X) != 0)
		{
			tsize = size;
			taddr = addr;
		}
		else if ((ph->p_flags & PF_W) != 0)
		{
			dsize = size;
			daddr = addr;
		}
	}
	bsize = 0;

	/* touchup process information -- vm system is unfinished! */
	vs->vm_tsize = btoc(tsize);			/* text size (pages) XXX */
	vs->vm_dsize = btoc(dsize + bsize);	/* data size (pages) XXX */
	vs->vm_taddr = (caddr_t)taddr;		/* user virtual address of text XXX */
	vs->vm_daddr = (caddr_t)daddr;		/* user virtual address of data XXX */

	rv = 0;

error:
	free(phdr, M_TEMP);

	return rv;
}

#if 0	// execv_elf32
static int execv_elf32(struct proc* p, struct execv_params* eprms, struct execve_args* uap)
{
	struct nameidata* 	ndp;
	struct vmspace*		vs = p->p_vmspace;
	char**				argbuf;
	int					rv, foff;
	vm_offset_t			newframe;
	struct exec*		hdr;

	ndp = &eprms->nd;
	hdr = &eprms->hdr;

	/* ELFヘッダーのチェック */
	if(hdr->e_shnum > ELF_MAXSHNUM || hdr->e_phnum > ELF_MAXPHNUM)
	{
		return (ENOEXEC);
	}

	/*※※※※※※※※※※ ELFヘッダーのチェックは未完成 ※※※※※※※※※※*/

	rv = alloc_new_stack(p, uap, &argbuf);
	if (rv != 0)
	{
		goto exec_fail;
	}

	rv = load_elf32(p, eprms);
	if (rv != 0)
	{
		goto exec_abort;
	}

	prepare_proc(p, eprms, (caddr_t)eprms->hdr.e_entry, (caddr_t)argbuf);

	return (0);
	
exec_fail:
	/* release namei request */
	nameiexec(ndp, (struct proc *)0);

	return(rv);

exec_abort:
	/* sorry, no more process anymore. exit gracefully */

	/* release namei request */
	nameiexec(ndp, (struct proc *)0);

	exit(p, W_EXITCODE(0, SIGABRT));

	/* NOTREACHED */
	return(0);
}
#endif

/*
 * execve() system call.
 */

/* ARGSUSED */
int execve(struct proc* p, struct execve_args* uap, void* retval)
{
	struct nameidata	*ndp;
	struct execv_params	eprms;
	struct vmspace*		vs;
	struct exec*		hdr;
	char**				argbuf;
	int					rv;
	vm_offset_t			newframe;
	caddr_t				entry;

	vs = p->p_vmspace;
	ndp = &eprms.nd;
	ndp->ni_nameiop = LOOKUP | LOCKLEAF | FOLLOW | SAVENAME;
	ndp->ni_segflg = UIO_USERSPACE;
	ndp->ni_dirp = uap->fname;

	/* is it there? */
	rv = namei(ndp, p);
	if (rv != 0)
	{
		return (rv);
	}

	if ((ndp->ni_vp->v_mount->mnt_flag & MNT_NOEXEC) != 0)
	{
		rv = EACCES;
		goto exec_fail;
	}

	rv = check_execv(p, &eprms);
	ON_ERROR_GOTO(rv != 0, exec_fail)

	/* read the header at the front of the file */
	rv = exec_read(p, ndp->ni_vp, 0, &eprms.hdr,
				   sizeof(eprms.hdr), IO_NODELOCKED);
	ON_ERROR_GOTO(rv != 0, exec_fail)

	hdr = &eprms.hdr;

	if (IS_AOUT(*hdr))		// aoutフォーマットの場合
	{
		rv = check_aout_hdr(p, &eprms);
		ON_ERROR_GOTO(rv != 0, exec_fail)

		rv = alloc_new_stack(p, uap, &argbuf);
		ON_ERROR_GOTO(rv != 0, exec_fail)

		rv = load_aout(p, &eprms);
		ON_ERROR_GOTO(rv != 0, exec_abort)

		entry = (caddr_t)eprms.hdr.a_entry;
	}
	else if (IS_ELF(*hdr))	// ELFフォーマットの場合
	{
		/* ELFヘッダーのチェック */
		ON_ERROR_GOTO((hdr->e_shnum > ELF_MAXSHNUM ||
					   hdr->e_phnum > ELF_MAXPHNUM), exec_fail);

		/*※※※※※※※※※※ ELFヘッダーのチェックは未完成 ※※※※※※※※※※*/

		rv = alloc_new_stack(p, uap, &argbuf);
		ON_ERROR_GOTO(rv != 0, exec_fail)

		rv = load_elf32(p, &eprms);
		ON_ERROR_GOTO(rv != 0, exec_abort)

		entry = (caddr_t)eprms.hdr.e_entry;
	}
	else	// ファイルフォーマットが未知の場合
	{
		rv = ENOEXEC;	// 実行できません...m(_ _)m
		goto exec_fail;	// メモリをいじっていないのでexec_failでよい。
	}

	prepare_proc(p, &eprms, entry, (caddr_t)argbuf);

	return (0);

exec_fail:
	nameiexec(ndp, (struct proc *)0);

	return (rv);

exec_abort:
	nameiexec(ndp, (struct proc *)0);
	exit(p, W_EXITCODE(0, SIGABRT));

	return (rv);	// 絶対に到達しませんが、コンパイラの警告を避けるため...
}
