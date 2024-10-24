/*-
 * Copyright (c) 1986, 1989, 1991 The Regents of the University of California.
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
 *	$Id: proc.h,v 1.1 95/02/25 22:35:09 bill Exp Locker: bill $
 */

#ifndef _PROC_H_
#define	_PROC_H_

#include <sys/time.h>
#include "ucred.h"
#include <machine/proc.h>		/* machine-dependent proc substruct */

/*
 * One structure allocated per session.
 */
struct	session {
	int	s_count;		/* ref cnt; pgrps in session */
	struct	proc *s_leader;		/* session leader */
	struct	vnode *s_ttyvp;		/* vnode of controlling terminal */
	struct	tty *s_ttyp;		/* controlling terminal */
	char	s_login[MAXLOGNAME];	/* setlogin() name */
};

/*
 * One structure allocated per process group.
 */
struct	pgrp {
	struct	pgrp *pg_hforw;		/* forward link in hash bucket */
	struct	proc *pg_mem;		/* pointer to pgrp members */
	struct	session *pg_session;	/* pointer to session */
	pid_t	pg_id;			/* pgrp id */
	int	pg_jobc;	/* # procs qualifying pgrp for job control */
};

/*
 * Description of a process.
 * This structure contains the information needed to manage a thread
 * of control, known in UN*X as a process; it has references to substructures
 * containing descriptions of things that the process uses, but may share
 * with related processes.  The process structure and the substructures
 * are always addressible except for those marked "(PROC ONLY)" below,
 * which might be addressible only on a processor on which the process
 * is running.
 */
#pragma pack(1)
struct	proc {
	struct	proc *p_link;		/* 0   doubly-linked run/sleep queue */
	struct	proc *p_rlink;		/* 4 */
	struct	proc *p_nxt;		/* 8   linked list of active procs */
	struct	proc **p_prev;		/* 12   and zombies */

	/* substructures: */
	struct	filedesc *p_fd;		/* 16  ptr to open files structure */
	struct	pstats *p_stats;	/* 20  accounting/statistics (PROC ONLY) */
	struct	plimit *p_limit;	/* 24  process limits */
	struct	vmspace *p_vmspace;	/* 28  address space */
	struct	sigacts *p_sigacts;	/* 32  signal actions, state (PROC ONLY) */

#define	p_ucred		p_cred->pc_ucred
#define	p_rlimit	p_limit->pl_rlimit

	int		p_flag;				/* 36  */
	char	p_stat;				/* 40  */
/*	char	p_space; */

	pid_t	p_pid;				/* 41  unique process id */
	struct	proc *p_hash;		/* 43  hashed based on p_pid for kill+exit+... */
	struct	proc *p_pptr;		/* 47  pointer to process structure of parent */
	struct	proc *p_osptr;		/* 51  pointer to older sibling processes */

/* The following fields are all zeroed upon creation in fork */
#define	p_startzero	p_ysptr
	struct	proc *p_ysptr;		/* 55  pointer to younger siblings */
	struct	proc *p_cptr;		/* 59  pointer to youngest living child */

	/* scheduling */
	u_int	p_cpu;				/* 63  cpu usage for scheduling */
	int		p_cpticks;			/* 67  ticks of cpu time */
	fixpt_t	p_pctcpu;			/* 71  %cpu for this process during p_time */
	caddr_t p_wchan;			/* 75  event process is awaiting */
	u_int	p_time;				/* 79  resident/nonresident time for swapping */
	u_int	p_slptime;			/* 83  time since last block */

	struct	itimerval p_realtimer;	/* 87  alarm timer */
	struct	timeval p_utime;	/* 103 user time */
	struct	timeval p_stime;	/* 111 system time */

	int		p_traceflag;		/* 119 kernel trace points */
	struct	vnode *p_tracep;	/* 123 trace to vnode */

	int		p_sig;				/* 127 signals pending to this process */

/* end area that is zeroed on creation */
#define	p_endzero	p_startcopy

/* The following fields are all copied upon creation in fork */
	sigset_t p_sigmask;			/* 131 current signal mask */
#define	p_startcopy	p_sigmask
	sigset_t p_sigignore;		/* 135 signals being ignored */
	sigset_t p_sigcatch;		/* 139 signals being caught by user */

	u_char	p_pri;				/* 143 priority, negative is high */
	u_char	p_usrpri;			/* 144 user-priority based on p_cpu and p_nice */
	char	p_nice;				/* 145 nice for cpu usage */
/*	char	p_space1; */

	struct 	pgrp *p_pgrp;		/* 146 pointer to process group */
	struct	proc *p_pgrpnxt;	/* 150 pointer to next process in process group */
	struct	pcred *p_cred;		/* 154 process owner's identity */
	char	p_comm[MAXCOMLEN+1];/* 158 */

/* end area that is copied on creation */
#define	p_endcopy	p_wmesg
	char	*p_wmesg;			/* 175 reason for sleep */
	/* int	p_thread; */		/* id for this "thread" (Mach glue) XXX */
	struct	user *p_addr;		/* 179 kernel virtual addr of u-area (PROC ONLY) */
	/* swblk_t	p_swaddr; */	/* disk address of u area when swapped */
	struct	mdproc p_md;		/* 183 any machine-dependent fields */

	/* u_short	p_xstat; */	/* Exit status for wait; also stop signal */
	short	p_dupfd;			/* 199 sideways return value from fdopen XXX */
	/* u_short	p_acflag; */	/* accounting flags */
	u_short	p_stksz;			/* 201 size of per process at p_addr */

	long	p_spare[6];			/* 203 tmp spares to avoid shifting eproc */
};
#pragma pack()

#define	p_session	p_pgrp->pg_session
#define	p_pgid		p_pgrp->pg_id

/*
 * Shareable credentials of process (always resident).
 * This includes a reference to the current user credentials used by
 * kernel services, as well as real and saved ids that may be used
 * to change ids.
 */
struct	pcred {
	struct	ucred *pc_ucred;	/* current credentials */
	uid_t	p_ruid;				/* real user id */
	uid_t	p_svuid;			/* saved effective user id */
	gid_t	p_rgid;				/* real group id */
	gid_t	p_svgid;			/* saved effective group id */
	int		p_refcnt;			/* number of references */
};

/* stat codes */
#define	SSLEEP	1		/* awaiting an event */
#define	SWAIT	2		/* (abandoned state) */
#define	SRUN	3		/* running */
#define	SIDL	4		/* intermediate state in process creation */
#define	SZOMB	5		/* intermediate state in process termination */
#define	SSTOP	6		/* process being traced */

/* flag codes */
#define	SLOAD	0x0000001	/* process context is resident */
#define	SSYS	0x0000002	/* bootstrap processes (swapper & pager) */
#define	SSINTR	0x0000004	/* sleep is interruptible */
#define	SCTTY	0x0000008	/* has a controlling terminal */
#define	SPPWAIT	0x0000010	/* parent is waiting for child to exec/exit */
#define SEXEC	0x0000020	/* process called exec */
#define	STIMO	0x0000040	/* timed out during tsleep() */
#define	SSEL	0x0000080	/* selecting; wakeup/waiting danger */
#define	SWEXIT	0x0000100	/* working on exiting */
#define	SNOCLDSTOP \
		0x0000200	/* no SIGCHLD when children stop */
/* the following three should probably be changed into a hold count */
#define	SLOCK	0x0000400	/* process being swapped out */
#define	SKEEP	0x0000800	/* another flag to prevent swap out */
#define	SPHYSIO	0x0001000	/* doing physical i/o */
#define	STRC	0x0004000	/* process is being traced */
#define	SWTED	0x0008000	/* stopped/traced process been wait()ed for */
#define	SPAGE	0x0020000	/* process in page wait state */
#define	SADVLCK	0x0040000	/* process may hold a POSIX advisory lock */
#define	SZCHILD	0x0800000	/* process has a zombie child */

/*
 * Priorities.  Note that with 32 run queues, priorities that differ
 * by less than 4 (128/32) are actually identical.
 */
#define	PSWP	0
#define	PVM		4
#define	PINOD	8
#define	PRIBIO	16
#define	PVFS	20
#define	PSOCK	24
#define	PWAIT	32
#define	PLOCK	36
#define	PPAUSE	40
#define	PUSER	50
#define	MAXPRI	127		/* Priorities range from 0 through MAXPRI. */

#define	PRIMASK	0x07f
#define	PCATCH	0x100		/* OR'd with pri for tsleep to check signals */

#define	NZERO	0		/* default "nice" */

#ifdef KERNEL
/*
 * We use process IDs <= PID_MAX;
 * PID_MAX + 1 must also fit in a pid_t
 * (used to represent "no process group").
 */
#define	PID_MAX		30000
#define	NO_PID		30001

/* process id hash */
#define	PIDHASH(pid)	((pid) & pidhashmask)
extern	int pidhashmask;
extern	struct proc *pidhash[];
void enterpidhash(struct proc *p);
void leavepidhash(struct proc *p);
struct proc *pfind(pid_t pid);

/* enter process into the process id hash table so pfind() can locate it */
extern inline void
enterpidhash(struct proc *p)
{
	struct proc **hash = &pidhash[PIDHASH(p->p_pid)];

	p->p_hash = *hash;
	*hash = p;
}

/* process to leave pid locate hash table, and is no longer pfind()able. */
extern inline void
leavepidhash(struct proc *p)
{
	struct proc **pp;
	extern void panic(const char *);

	for (pp = &pidhash[PIDHASH(p->p_pid)]; *pp; pp = &(*pp)->p_hash)
		if (*pp == p) {
			*pp = p->p_hash;
			return;
		}
#ifdef	DIAGNOSTIC
	panic("leavepidhash");
#endif
}

/* Locate a process by id, returning process pointer if valid process. */
extern inline struct proc *
pfind(pid_t pid)
{
	struct proc *p = pidhash[PIDHASH(pid)];

	for (; p; p = p->p_hash)
		if (p->p_pid == pid)
			return (p);
	return ((struct proc *)0);
}

/* process sessions */
#define SESS_LEADER(p)	((p)->p_session->s_leader == (p))
#define	SESSHOLD(s)	((s)->s_count++)
#define	SESSRELE(s)	{ \
		if (--(s)->s_count == 0) \
			FREE(s, M_SESSION); \
	}

/* process groups */
extern	struct pgrp *pgrphash[];
struct pgrp *pgfind(pid_t pgid);
void enterpgrp(struct proc *p, pid_t pgid, int mksess);
void leavepgrp(struct proc *p);
void pgdelete(struct pgrp *pgrp);
void fixjobc(struct proc *p, struct pgrp *pgrp, int entering);
void pgrpdump(void);

int inferior(struct proc *p);
void roundrobin(void);
void schedcpu(void);
void updatepri(struct proc *p);
void unsleep(struct proc *p);
void rqinit(void);
void setrun(struct proc *p);
void setpri(struct proc *p);

/* process credentials */

/* process creation/destruction */
int fork1(struct proc *p1, int isvfork, int *retval);
void exit(struct proc *p, int rv);

extern struct		proc *zombproc, *allproc;	/* lists of procs in various states */
extern struct		proc proc0;					/* process slot for swapper */
extern struct		proc *initproc;				/* process slots for init, pager */
extern int			nprocs;						/* current number of procs */
extern const int	maxproc;					/* max number of procs */

#define	NQS	32			/* 32 run queues */
#define	PPQ	((MAXPRI+1) / NQS)	/* priorities per queue */
extern struct	prochd {
	struct	proc *ph_link;	/* linked list of running processes */
	struct	proc *ph_rlink;
} qs[NQS];

extern int	whichqs;		/* bit mask summarizing non-empty qs's */

/* interface symbols */
#define	__ISYM_VERSION__ "1"	/* XXX RCS major revision number of hdr file */
#include "isym.h"		/* this header has interface symbols */

/* global variables used in core kernel and other modules */
__ISYM__(struct proc *, curproc,)
__ISYM__(struct proc *, pageproc,)

/* functions used in modules */
__ISYM__(void, wakeup, (caddr_t chan))
__ISYM__(int, tsleep, (caddr_t chan, int pri, char *wmesg, int timo))
__ISYM__(struct pcred *, modpcred, (struct proc *))

#undef __ISYM__
#undef __ISYM_ALIAS__
#undef __ISYM_VERSION__
#endif	/* KERNEL */

#endif	/* !_PROC_H_ */
